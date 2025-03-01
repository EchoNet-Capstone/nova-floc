import sys
import clang.cindex
from clang.cindex import CursorKind, TypeKind, Config, Cursor, Type
import re

# --- Configuration ---
FLOC_HPP = sys.argv[1]  # Input C++ header file
FLOC_PY = sys.argv[2]   # Output Python file

# --- Set libclang path (ADJUST AS NEEDED) ---
Config.set_library_file('/usr/lib/x86_64-linux-gnu/libclang-14.so')  # Use your actual path


def get_scapy_field_type(clang_type: Type) -> str:
    """Maps Clang types to Scapy field types."""
    clang_type = clang_type.get_canonical()

    if clang_type.kind in (TypeKind.UCHAR, TypeKind.CHAR_U, TypeKind.CHAR_S):
        return "ByteField"
    elif clang_type.kind in (TypeKind.USHORT, TypeKind.SHORT):
        return "ShortField"
    elif clang_type.kind in (TypeKind.UINT, TypeKind.INT):
        return "IntField"
    elif clang_type.kind == TypeKind.POINTER:
        return "StrFixedLenField"
    elif clang_type.kind == TypeKind.ENUM:
        return "ByteEnumField"
    elif clang_type.kind == TypeKind.CONSTANTARRAY:
        element_type = clang_type.get_array_element_type().get_canonical()
        array_size = clang_type.get_array_size()
        if element_type.kind in (TypeKind.CHAR_S, TypeKind.CHAR_U, TypeKind.UCHAR):
            return f"StrFixedLenField * {array_size}"
        else:
            element_scapy_type = get_scapy_field_type(element_type)
            return f"{element_scapy_type} * {array_size}"
    elif clang_type.kind == TypeKind.RECORD:
        return clang_type.get_declaration().spelling.replace("_t", "")
    elif clang_type.kind == TypeKind.TYPEDEF:
        underlying = clang_type.get_declaration().underlying_typedef_type.get_canonical()
        return get_scapy_field_type(underlying)
    else:
        raise ValueError(f"Unsupported Clang type: {clang_type.spelling}")


def generate_scapy_class(cursor: Cursor, all_structs: dict) -> tuple[str, str] | tuple[None, None]:
    """Generates a Scapy class definition for a struct."""
    if cursor.spelling == "":
        return None, None

    class_name = cursor.spelling.replace("_t", "")
    fields = []

    for child in cursor.get_children():
        if child.kind == CursorKind.FIELD_DECL:
            field_name = child.spelling
            field_type = child.type
            bit_width = child.get_bitfield_width() if child.is_bitfield() else None

            if field_type.kind == TypeKind.TYPEDEF:
                resolved = field_type.get_declaration().underlying_typedef_type.get_canonical()
                if resolved.kind == TypeKind.RECORD and resolved.get_declaration().kind == CursorKind.UNION_DECL:
                    union_class_name = resolved.get_declaration().spelling.replace("_t", "")
                    fields.append(f"PacketField('{field_name}', {union_class_name}(), {union_class_name})")
                    continue

            if field_type.kind == TypeKind.RECORD:
                decl = field_type.get_declaration()
                if decl.kind == CursorKind.UNION_DECL:
                    for union_member in decl.get_children():
                        if union_member.kind == CursorKind.FIELD_DECL:
                            nested_struct_name = union_member.type.get_declaration().spelling.replace("_t", "")
                            if nested_struct_name == "":
                                nested_struct_name = all_structs.get(union_member.type.get_declaration().spelling + "_t", {}).get("spelling", "").replace("_t", "")
                            if nested_struct_name != "":
                                fields.append(f"PacketField('{union_member.spelling}', {nested_struct_name}(), {nested_struct_name})")
                    continue
                elif decl.kind == CursorKind.STRUCT_DECL:
                    nested_struct_name = field_type.get_declaration().spelling.replace("_t", "")
                    fields.append(f"PacketField('{field_name}', {nested_struct_name}(), {nested_struct_name})")
                    continue

            scapy_type_str = get_scapy_field_type(field_type)

            if " * " in scapy_type_str:
                element_type, array_size_str = scapy_type_str.split(" * ")
                if element_type == "StrFixedLenField":
                    fields.append(f"StrFixedLenField('{field_name}', b'', length={int(array_size_str)})")
                else:
                    fields.append(f"{element_type}('{field_name}', [0] * {int(array_size_str)})")
            elif scapy_type_str == "ByteEnumField":
                enum_dict = {}
                for enum_child in field_type.get_declaration().get_children():
                    if enum_child.kind == CursorKind.ENUM_CONSTANT_DECL:
                        enum_dict[enum_child.enum_value] = enum_child.spelling.encode()
                fields.append(f"ByteEnumField('{field_name}', 0, {enum_dict})")
            elif bit_width is not None:
                fields.append(f"BitField('{field_name}', 0, {bit_width})")
            else:
                fields.append(f"{scapy_type_str}('{field_name}', 0)")

    fields_str = ",\n        ".join(fields)
    scapy_class = f"""
class {class_name}(Packet):
    name = "{class_name}"
    fields_desc = [
        {fields_str}
    ]
"""
    return scapy_class, class_name


def generate_union_class(cursor: Cursor) -> tuple[str, str] | tuple[None, None]:
    """Generates Scapy class for standalone unions."""
    if cursor.spelling == "":
        return None, None

    union_name = cursor.spelling.replace("_t", "")
    fields = []
    for member in cursor.get_children():
        if member.kind == CursorKind.FIELD_DECL:
            member_name = member.spelling
            member_type = member.type

            if member_type.kind == TypeKind.CONSTANTARRAY:
                element_type = member_type.get_array_element_type().get_canonical()
                array_size = member_type.get_array_size()
                if element_type.kind in (TypeKind.CHAR_S, TypeKind.CHAR_U, TypeKind.UCHAR):
                    fields.append(f"StrFixedLenField('{member_name}', b'', length={array_size})")
                else:
                    element_scapy_type = get_scapy_field_type(element_type)
                    fields.append(f"{element_scapy_type}('{member_name}', [0] * {array_size})")
            elif member_type.kind == TypeKind.RECORD:
                member_type_name = member.type.get_declaration().spelling.replace("_t", "")
                if member_type_name == "":
                     member_type_name = member.type.get_declaration().type.get_declaration().spelling.replace("_t", "")
                fields.append(f"PacketField('{member_name}', {member_type_name}(), {member_type_name})")
            else:
                fields.append(f"{get_scapy_field_type(member_type)}('{member_name}', 0)")

    fields_str = ",\n        ".join(fields)
    scapy_class = f"""
class {union_name}(Packet):
    name = "{union_name}"
    fields_desc = [
        {fields_str}
    ]
"""
    return scapy_class, union_name

def generate_enum_constants_and_functions(tu: clang.cindex.TranslationUnit, all_structs: dict) -> str:
    """Generates Python constants and conversion functions for enums."""
    code = ""
    enum_mappings = {}  # Store mappings for generating functions

    # Now, find enums at the *top level* (not inside structs)
    for cursor in tu.cursor.get_children():
        if cursor.kind == CursorKind.ENUM_DECL:
            enum_name = cursor.spelling
            if not enum_name:  # Handle anonymous enums
                continue
            enum_name = enum_name.replace("_e", "") #remove _e from the name
            #func_name_base = enum_name.lower() #WRONG.
            #Correct function name base: structname_fieldname
            func_name_base = '_'.join(re.findall(r'[A-Z][a-z]*', enum_name)).lower() #CORRECT


            constants = []
            mapping = {}
            for enum_const in cursor.get_children():
                if enum_const.kind == CursorKind.ENUM_CONSTANT_DECL:
                    const_name = enum_const.spelling
                    const_value = enum_const.enum_value
                    constants.append(f"{const_name} = b'{const_name}'")  # Define as bytes
                    mapping[const_value] = const_name

            enum_mappings[func_name_base] = mapping  # Store for function generation
            code += "\n".join(constants) + "\n\n"



    # Generate conversion functions
    for func_name_base, mapping in enum_mappings.items():
        func_name = f"get_{func_name_base}"
        code += f"def {func_name}(value: int) -> bytes:\n"
        code += f'    """Converts an integer value to the {func_name_base} enum\'s byte string representation."""\n'
        code += f"    _mapping = {{\n"
        for num_val, byte_val in mapping.items():
            code += f"        {num_val}: {byte_val},\n"
        code += f"    }}\n"
        code += f"    return _mapping.get(value, b'')\n\n"  # Return empty bytes if not found

    return code


def generate_bind_layers_for_enum(header_cursor, discriminator_field_name, header_name, all_structs, prefix, remove_str, prefix_repl=""):
    """Generates bind_layers calls."""
    bind_layers_code = ""
    if not header_cursor:
        return ""

    enum_cursor = None
    for child in header_cursor.get_children():
        if child.spelling == discriminator_field_name:
            enum_cursor = child.type.get_declaration()
            break

    if not enum_cursor:
        return ""

    for enum_const in enum_cursor.get_children():
        if enum_const.kind == CursorKind.ENUM_CONSTANT_DECL:
            enum_name = enum_const.spelling
            enum_value = enum_const.enum_value

            struct_name = prefix_repl + enum_name.replace(prefix, "").replace(remove_str, "").capitalize() + "Header"
            if struct_name + "_t" in all_structs:
                bind_layers_code += f"bind_layers({header_name}, {struct_name}, {discriminator_field_name}={enum_value})\n"
            elif header_name == "SerialFlocHeader":
                if "BROADCAST" in enum_name:
                     bind_layers_code += f"bind_layers(SerialFlocPacket, SerialBroadcastPacket, type={enum_value})\n"
                elif "UNICAST" in enum_name:
                     bind_layers_code += f"bind_layers(SerialFlocPacket, SerialUnicastPacket, type={enum_value})\n"

    return bind_layers_code


def generate_bind_layers(all_structs):
    """Generates bind_layers calls."""
    bind_layers_code = ""
    bind_layers_code += generate_bind_layers_for_enum(
        all_structs.get("HeaderCommon_t"), "type", "HeaderCommon", all_structs, "FLOC_", "_TYPE"
    )
    bind_layers_code += generate_bind_layers_for_enum(
        all_structs.get("SerialFlocHeader_t"), "type", "SerialFlocHeader", all_structs, "SERIAL_", "_TYPE", "Serial"
    )
    return bind_layers_code

def main():
    """Main function."""
    index = clang.cindex.Index.create()
    tu = index.parse(FLOC_HPP, options=clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

    all_structs = {}
    for cursor in tu.cursor.get_children():
        if cursor.kind in (CursorKind.STRUCT_DECL, CursorKind.UNION_DECL, CursorKind.TYPEDEF_DECL):
            all_structs[cursor.spelling] = cursor

    scapy_classes = []
    for struct_name, struct_cursor in all_structs.items():
        if struct_cursor.kind == CursorKind.TYPEDEF_DECL:
            underlying = struct_cursor.underlying_typedef_type.get_canonical()
            if underlying.kind == TypeKind.RECORD:
                decl_kind = underlying.get_declaration().kind
                if decl_kind == CursorKind.UNION_DECL:
                    scapy_class, classname = generate_union_class(underlying.get_declaration())
                    if scapy_class:
                        scapy_classes.append(scapy_class)
                elif decl_kind == CursorKind.STRUCT_DECL:
                    scapy_class, classname = generate_scapy_class(underlying.get_declaration(), all_structs)
                    if scapy_class:
                        scapy_classes.append(scapy_class)
            continue

        if struct_cursor.kind == CursorKind.STRUCT_DECL:
            scapy_class, classname = generate_scapy_class(struct_cursor, all_structs)
            if scapy_class:
                scapy_classes.append(scapy_class)

    enum_constants_and_functions = generate_enum_constants_and_functions(tu, all_structs)
    bind_layers_code = generate_bind_layers(all_structs)


    final_output = f"""\
# This file is generated automatically by the FLOC generator script.
#
# Usage:
#   - Import this module to access the generated Scapy classes.
#   - Use these classes to build, send, and dissect FLOC packets.
#
from scapy.all import Packet, BitField, ShortField, ByteField, IntField, StrFixedLenField, PacketField, bind_layers, ByteEnumField, ConditionalField, RawVal

{enum_constants_and_functions}

{''.join(scapy_classes)}
{bind_layers_code}
"""
    with open(FLOC_PY, 'w') as f:
        f.write(final_output)


if __name__ == "__main__":
    main()