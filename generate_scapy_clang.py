#!/usr/bin/env python3
import sys
import clang.cindex
import re
from clang.cindex import CursorKind, TypeKind, Config

# --- Set libclang path (adjust as needed) ---
Config.set_library_file('/usr/lib/x86_64-linux-gnu/libclang-14.so')

# --- Types we want to ignore ---
IGNORE_TYPES = {"FlocPacketVariant_u", "FlocPacket_t", "SerialFlocPacketVariant_u", "SerialFlocPacket"}

# Global list to track FLOC packet classes for bind_layers automation
floc_packet_classes = []

def camel_to_snake(name: str) -> str:
    """Convert CamelCase to snake_case."""
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s1).lower()

def get_scapy_field_type(clang_type, is_in_data_packet=False, field_name=None) -> str:
    """Map a Clang type to a Scapy field type."""
    clang_type = clang_type.get_canonical()
    if clang_type.kind in (TypeKind.UCHAR, TypeKind.CHAR_U, TypeKind.CHAR_S):
        return "ByteField"
    elif clang_type.kind in (TypeKind.USHORT, TypeKind.SHORT):
        return "ShortField"
    elif clang_type.kind in (TypeKind.UINT, TypeKind.INT):
        return "IntField"
    elif clang_type.kind == TypeKind.POINTER:
        return "StrLenField"
    elif clang_type.kind == TypeKind.ENUM:
        return "ByteEnumField"
    elif clang_type.kind == TypeKind.CONSTANTARRAY:
        element_type = clang_type.get_array_element_type().get_canonical()
        if element_type.kind in (TypeKind.CHAR_S, TypeKind.CHAR_U, TypeKind.UCHAR):
            if is_in_data_packet and field_name == "data":
                return "StrLenField"
            return "StrFixedLenField"
        else:
            return "FieldListField"
    elif clang_type.kind == TypeKind.RECORD:
        # Return the name of the record (stripping _t)
        return clang_type.get_declaration().spelling.replace("_t", "")
    elif clang_type.kind == TypeKind.TYPEDEF:
        underlying = clang_type.get_declaration().underlying_typedef_type.get_canonical()
        return get_scapy_field_type(underlying, is_in_data_packet, field_name)
    else:
        raise ValueError(f"Unsupported Clang type: {clang_type.spelling}")

def generate_field(child, is_in_data_packet=False, class_name="") -> str | None:
    """Generate a single field line for a Scapy class."""
    field_name = child.spelling
    field_type = child.type
    bitfield_width = child.get_bitfield_width() if child.is_bitfield() else None

    # Skip fields whose type is one of our ignored wrapper types.
    type_decl = field_type.get_declaration()
    if type_decl.spelling in IGNORE_TYPES:
        return None

    scapy_type = get_scapy_field_type(field_type, is_in_data_packet, field_name)
    # If the field type name ends with "Header", use PacketField
    if scapy_type.endswith("Header"):
        return f"PacketField('{field_name}', {scapy_type}(), {scapy_type})"
    # For variable-length data fields
    if scapy_type == "StrLenField":
        if is_in_data_packet and field_name == "data":
            return f"StrLenField('{field_name}', b'', length_from=lambda pkt: pkt.header.size)"
        else:
            return None
    if scapy_type == "StrFixedLenField":
        array_size = field_type.get_array_size()
        return f"StrFixedLenField('{field_name}', b'', length={array_size})"
    if scapy_type == "ByteEnumField":
        # Build enum dictionary from the enum declaration.
        enum_decl = field_type.get_declaration()
        enum_dict = {}
        for enum_child in enum_decl.get_children():
            if enum_child.kind == CursorKind.ENUM_CONSTANT_DECL:
                enum_dict[enum_child.enum_value] = enum_child.spelling
        # For FlocHeader, use BitEnumField with 4 bits for field 'type'
        if class_name == "FlocHeader" and field_name == "type":
            return f"BitEnumField('{field_name}', 0, 4, {enum_dict})"
        # Otherwise (for SerialFlocHeader) use ByteEnumField.
        return f"ByteEnumField('{field_name}', 0, {enum_dict})"
    if bitfield_width is not None:
        return f"BitField('{field_name}', 0, {bitfield_width})"
    return f"{scapy_type}('{field_name}', 0)"

def generate_scapy_class(cursor) -> tuple[str, str] | tuple[None, None]:
    """Generate a Scapy class definition from a struct/typedef."""
    if not cursor.spelling or cursor.spelling.strip() == "":
        return None, None
    if cursor.spelling in IGNORE_TYPES:
        return None, None
    class_name = cursor.spelling.replace("_t", "")
    fields = []
    # Determine if this is a data packet (i.e. one with variable length data)
    is_data_packet = class_name in ("DataPacket", "CommandPacket", "ResponsePacket")
    # Special handling for SerialBroadcastPacket and SerialUnicastPacket: skip unwanted fields.
    if class_name in ("SerialBroadcastPacket", "SerialUnicastPacket"):
        for child in cursor.get_children():
            if class_name == "SerialUnicastPacket" and child.spelling == "dest_addr":
                line = generate_field(child, is_data_packet, class_name)
                if line:
                    fields.append(line)
        # For SerialBroadcastPacket, we intentionally leave the fields list empty.
    else:
        for child in cursor.get_children():
            if child.kind == CursorKind.FIELD_DECL:
                line = generate_field(child, is_data_packet, class_name)
                if line:
                    fields.append(line)
    fields_str = ",\n        ".join(fields)
    class_def = f"""
class {class_name}(Packet):
    name = "{class_name}"
    fields_desc = [
        {fields_str}
    ]
"""
    # Record FLOC packets for bind_layers automation.
    if class_name in ("DataPacket", "CommandPacket", "AckPacket", "ResponsePacket"):
        floc_packet_classes.append(class_name)
    return class_def, class_name

def generate_enum_constants_and_functions(tu) -> str:
    """Generate enum constants and conversion functions."""
    code = ""
    for cursor in tu.cursor.get_children():
        if cursor.kind == CursorKind.ENUM_DECL and cursor.spelling:
            enum_name = cursor.spelling
            # Remove trailing _e if present.
            enum_name_clean = enum_name[:-2] if enum_name.endswith("_e") else enum_name
            # Determine function name and docstring base by convention.
            if enum_name_clean.lower() == "flocpackettype":
                func_name = "get_floc_packet_type"
                doc_base = "floc_packet_type"
            elif enum_name_clean.lower() == "commandtype":
                func_name = "get_command_type"
                doc_base = "command_type"
            elif enum_name_clean.lower() == "serialflocpackettype":
                func_name = "get_serial_floc_packet_type"
                doc_base = "serial_floc_packet_type"
            else:
                func_name = f"get_{camel_to_snake(enum_name_clean)}"
                doc_base = camel_to_snake(enum_name_clean)
            mapping = {}
            for enum_child in cursor.get_children():
                if enum_child.kind == CursorKind.ENUM_CONSTANT_DECL:
                    const_name = enum_child.spelling
                    mapping[enum_child.enum_value] = const_name
                    code += f"{const_name} = '{const_name}'\n"
            code += "\n"
            code += f"def {func_name}(value: int) -> str:\n"
            code += f'    """Converts an integer value to the {doc_base} enum\'s string representation."""\n'
            code += f"    _mapping = {{\n"
            for num, const_name in mapping.items():
                code += f"        {num}: {const_name},\n"
            code += f"    }}\n"
            code += f"    return _mapping.get(value, '')\n\n"
    return code

def generate_bind_layers_code() -> str:
    """Automatically generate bind_layers calls based on collected FLOC packet classes."""
    bind_code = ""
    for packet in floc_packet_classes:
        bind_code += f"bind_layers(SerialFlocHeader, {packet}, type=66)  # Broadcast\n"
    for packet in floc_packet_classes:
        bind_code += f"bind_layers(SerialFlocHeader, {packet}, type=85)  # Unicast\n"
    # Also bind SerialBroadcastPacket and SerialUnicastPacket to Raw.
    bind_code += "bind_layers(SerialBroadcastPacket, Raw)\n"
    bind_code += "bind_layers(SerialUnicastPacket, Raw)\n"
    return bind_code

def main():
    FLOC_HPP = sys.argv[1]  # Input header file
    FLOC_PY = sys.argv[2]   # Output Python file
    index = clang.cindex.Index.create()
    tu = index.parse(FLOC_HPP, options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
    
    # Gather all structs, unions, and typedefs (only those with non-empty spelling)
    structs = {}
    for cursor in tu.cursor.get_children():
        if cursor.kind in (CursorKind.STRUCT_DECL, CursorKind.UNION_DECL, CursorKind.TYPEDEF_DECL):
            if cursor.spelling and cursor.spelling.strip():
                structs[cursor.spelling] = cursor

    scapy_classes = []
    # Process typedefs first.
    for name, cursor in structs.items():
        if cursor.kind == CursorKind.TYPEDEF_DECL:
            underlying = cursor.underlying_typedef_type.get_canonical()
            if underlying.kind == TypeKind.RECORD:
                decl = underlying.get_declaration()
                if decl.spelling in IGNORE_TYPES:
                    continue
                class_def, _ = generate_scapy_class(decl)
                if class_def:
                    scapy_classes.append(class_def)
    # Process structs.
    for name, cursor in structs.items():
        if cursor.kind == CursorKind.STRUCT_DECL and name not in IGNORE_TYPES:
            class_def, _ = generate_scapy_class(cursor)
            if class_def:
                scapy_classes.append(class_def)
    # Optionally process unions if needed.
    for name, cursor in structs.items():
        if cursor.kind == CursorKind.UNION_DECL and name not in IGNORE_TYPES:
            class_def, _ = generate_scapy_class(cursor)
            if class_def:
                scapy_classes.append(class_def)
    
    enum_code = generate_enum_constants_and_functions(tu)
    bind_layers_code = generate_bind_layers_code()
    
    header_comment = "# floc_pkts.py (VARIABLE LENGTH DATA)"
    final_output = f"""{header_comment}
from scapy.all import Packet, BitField, ShortField, ByteField, IntField, StrLenField, PacketField, bind_layers, ByteEnumField, BitEnumField, Raw

{enum_code}
{''.join(scapy_classes)}

{bind_layers_code}
"""
    with open(FLOC_PY, 'w') as f:
        f.write(final_output)

if __name__ == "__main__":
    main()
