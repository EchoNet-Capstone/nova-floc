import sys
import clang.cindex
from clang.cindex import CursorKind, TypeKind, Config, Cursor, Type

FLOC_HPP = sys.argv[1]
FLOC_PY = sys.argv[2]

# --- Use Config.set_library_file ---
Config.set_library_file('/usr/lib/x86_64-linux-gnu/libclang-14.so')

def get_scapy_field_type(clang_type):
	"""Maps Clang types to Scapy field types."""
	clang_type = clang_type.get_canonical()
	if clang_type.kind == TypeKind.UCHAR or clang_type.kind == TypeKind.CHAR_U:
		return "ByteField"
	elif clang_type.kind == TypeKind.USHORT or clang_type.kind == TypeKind.SHORT:
		return "ShortField"
	elif clang_type.kind == TypeKind.UINT or clang_type.kind == TypeKind.INT:
		return "IntField"
	elif clang_type.kind == TypeKind.POINTER:
		return "StrField"
	elif clang_type.kind == TypeKind.ENUM:
		return "ByteEnumField"
	elif clang_type.kind == TypeKind.CONSTANTARRAY:
		element_type = clang_type.get_array_element_type()
		array_size = clang_type.get_array_size()
		element_scapy_type = get_scapy_field_type(element_type)
		return f"{element_scapy_type} * {array_size}"
	else:
		raise ValueError(f"Unsupported Clang type: {clang_type.spelling}")

def generate_scapy_class(cursor: Cursor):
	"""Generates Scapy class definition."""
	if cursor.spelling == "":
		return None, None

	class_name = cursor.spelling.replace("_t", "")
	fields = []
	base_class = "Packet"

	if not cursor.spelling == "HeaderCommon_t":
		for c in cursor.get_children():
			c: Cursor
			if c.spelling == "common":
				base_class = c.type.spelling.replace("_t", "")

	for child in cursor.get_children():
		if child.kind == CursorKind.FIELD_DECL:
			field_name = child.spelling
			field_type = child.type
			bit_width = child.get_bitfield_width() if child.is_bitfield() else None

			if field_type.kind == TypeKind.RECORD:
				# Handle nested structs and unions
				if child.type.get_declaration().kind == CursorKind.UNION_DECL:
					# Nested Union: Create PacketFields for each member
					for union_member in child.type.get_declaration().get_children():
						if union_member.kind == CursorKind.FIELD_DECL:
							nested_struct_name = union_member.type.get_declaration().spelling.replace("_t", "")
							fields.append(f"PacketField('{union_member.spelling}', {nested_struct_name}(), {nested_struct_name})")
				else: #Nested Struct
					nested_struct_name = field_type.get_declaration().spelling.replace("_t", "")
					fields.append(f"PacketField('{field_name}', {nested_struct_name}(), {nested_struct_name})")
				continue

			scapy_type_str = get_scapy_field_type(field_type)

			if " * " in scapy_type_str:
				element_type, array_size_str = scapy_type_str.split(" * ")
				array_size = int(array_size_str)
				fields.append(f"{element_type}('{field_name}', [0] * {array_size})")
			elif scapy_type_str == "ByteEnumField":
				enum_dict = {}
				for enum_child in field_type.get_declaration().get_children():
					if enum_child.kind == CursorKind.ENUM_CONSTANT_DECL:
						enum_dict[enum_child.spelling.encode()] = enum_child.enum_value
				fields.append(f"ByteEnumField('{field_name}', 0, {enum_dict})")
			elif bit_width is not None:
				fields.append(f"BitField('{field_name}', 0, {bit_width})")
			else:
				fields.append(f"{scapy_type_str}('{field_name}', 0)")

	fields_str = ",\n        ".join(fields)
	scapy_class = f"""
class {class_name}({base_class}):
	name = "{class_name}"
	fields_desc = [
		{fields_str}
	]
"""
	if base_class != "Packet":
		scapy_class += """
	def extract_padding(self, s):
		return "", s
"""
	return scapy_class, class_name

def generate_bind_layers_for_enum(header_cursor, discriminator_field_name, header_name, all_structs, prefix, remove_str, prefix_repl=""):
	"""Generates bind_layers calls for a given header and discriminator field."""

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
			struct_name = prefix_repl + enum_name.replace(prefix, "").replace(remove_str, "").capitalize() + "Header_t"
			if struct_name in all_structs:
				class_name = struct_name.replace("_t", "")
				bind_layers_code += f"bind_layers({header_name}, {class_name}, {discriminator_field_name}={enum_value})\n"

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
	# Add more calls to generate_bind_layers_for_enum as needed for other headers

	return bind_layers_code

def main():
	index = clang.cindex.Index.create()
	tu = index.parse(FLOC_HPP)

	all_structs = {}
	for cursor in tu.cursor.get_children():
		if cursor.kind == CursorKind.STRUCT_DECL or cursor.kind == CursorKind.UNION_DECL:
			all_structs[cursor.spelling] = cursor

	scapy_classes = []
	for struct_name, struct_cursor in all_structs.items():
		scapy_class, classname = generate_scapy_class(struct_cursor)
		if scapy_class:
			scapy_classes.append(scapy_class)

	bind_layers_code = generate_bind_layers(all_structs)

	final_output = f"""\
# This file is generated automatically by the FLOC generator script.
#
# Usage:
#   - Import this module to access the generated Scapy classes, which represent
#     the packet headers defined in the FLOC header file.
#   - The classes (e.g., HeaderCommon, CommandHeader, AckHeader, ResponseHeader) are
#     derived from Scapy's Packet class and can be used to build, send, and dissect packets.
#   - The bind_layers calls at the end automatically bind the correct header types
#     based on the 'type' field in HeaderCommon.
#
from scapy.all import Packet, BitField, ShortField, ByteField, IntField, StrField, PacketField, bind_layers, ByteEnumField

{''.join(scapy_classes)}
{bind_layers_code}
"""
	with open(FLOC_PY, 'w') as f:
		f.write(final_output)

if __name__ == "__main__":
	main()
