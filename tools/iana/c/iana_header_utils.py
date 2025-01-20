import os
import re
import time
import email
import requests

'''
MIT License

Copyright (c) 2023, Brian Khuu
All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
'''

###############################################################################
# CSV Handlers

def _download_csv(csv_url: str, cache_file: str) -> str:
    """Downloads CSV content from a URL and saves it to a cache file."""
    response = requests.get(csv_url)
    response.raise_for_status()

    csv_content = response.text
    os.makedirs(os.path.dirname(cache_file), exist_ok=True)
    with open(cache_file, "w", encoding="utf-8") as file:
        file.write(csv_content)

    return csv_content

def _read_cache_csv(cache_file: str) -> str:
    """Reads the cached CSV content from a file."""
    with open(cache_file, "r", encoding="utf-8") as file:
        return file.read()

def read_or_download_csv(csv_url: str, cache_file: str) -> str:
    """
    Fetches CSV content either from a URL or from a cache file.

    Will only download and overwrite the cache file if the remote file has changed since last download.
    """
    try:
        response = requests.head(csv_url)
        if not os.path.exists(cache_file) or 'last-modified' not in response.headers:
            return _download_csv(csv_url, cache_file)

        remote_last_modified = response.headers['last-modified']
        remote_timestamp = time.mktime(email.utils.parsedate_to_datetime(remote_last_modified).timetuple())
        cached_timestamp = os.path.getmtime(cache_file)
        if remote_timestamp > cached_timestamp:
            return _download_csv(csv_url, cache_file)

        return _read_cache_csv(cache_file)

    except requests.RequestException as err:
        if os.path.exists(cache_file):
            return _read_cache_csv(cache_file)
        raise Exception("Error fetching CSV and no cache available.") from err

###############################################################################
# C Code Generation Utilities

def get_content_of_typedef_enum(c_code: str, typedef_enum_name: str) -> str:
    match = re.search(fr'typedef enum [^{{]*\{{([^}}]*)\}} {typedef_enum_name};', c_code, flags=re.DOTALL)
    if not match:
        return None

    return match.group(1)

def override_enum_from_existing_typedef_enum(header_file_content: str, c_typedef_name: str, c_enum_list, depreciated_enum_support = True):
    """
    Check for existing enum so we do not break it
    """
    def extract_enum_values_from_typedef_enum(c_code: str, existing_enum_content: str) -> str:
        matches = re.findall(r'(\w+)\s*=\s*(\d+)', existing_enum_content)

        enum_values = {}
        for match in matches:
            enum_name, enum_value = match
            if int(enum_value) in enum_values:
                enum_values[int(enum_value)].append(enum_name)
            else:
                enum_values[int(enum_value)] = [enum_name]

        return enum_values

    existing_enum_content = get_content_of_typedef_enum(header_file_content, c_typedef_name)
    existing_enum_name_list = extract_enum_values_from_typedef_enum(header_file_content, existing_enum_content)
    for id_value, existing_enum_name_list_entry in sorted(existing_enum_name_list.items()):
        for existing_enum_name in existing_enum_name_list_entry:
            # Check if we already have a generated value for this existing entry
            if id_value in c_enum_list: # Override
                expected_enum_name = c_enum_list[id_value]["enum_name"]
                # Check if duplicated
                if existing_enum_name != expected_enum_name:
                    # Existing Enum Name Does Not Match With This Name
                    if depreciated_enum_support:
                        # Preserve But Mark As Depreciated / Backward Compatible
                        c_enum_list[id_value]["depreciated_enum_name"] = existing_enum_name
                    else:
                        # Preserve But Override
                        c_enum_list[id_value]["enum_name"] = existing_enum_name
            else: # Add
                c_enum_list[id_value] = {"enum_name" : existing_enum_name}
    return c_enum_list

def generate_c_enum_content(c_head_comment, c_enum_list, c_range_marker = None, spacing_string = "  ", int_suffix = ""):
    c_range_marker_index = 0
    def range_marker_render(c_range_marker, id_value=None):
        nonlocal c_range_marker_index
        if c_range_marker is None:
            return ''

        range_marker_content = ''
        while c_range_marker_index < len(c_range_marker):
            start_range = c_range_marker[c_range_marker_index].get("start")
            end_range = c_range_marker[c_range_marker_index].get("end")
            range_comment = c_range_marker[c_range_marker_index].get("description")
            if id_value is None or start_range <= id_value:
                range_marker_content += '\n' + spacing_string + f'/* {start_range}-{end_range} : {range_comment} */\n'
                c_range_marker_index += 1
                continue
            break

        return range_marker_content

    c_enum_content = c_head_comment

    for id_value, row in sorted(c_enum_list.items()):
        c_enum_content += range_marker_render(c_range_marker, id_value)
        if "comment" in row:
            c_enum_content += spacing_string + f'// {row.get("comment", "")}\n'
        c_enum_content += spacing_string + f'{row.get("enum_name", "")} = {id_value}{int_suffix}'
        if "depreciated_enum_name" in row:
            c_enum_content += ',\n' + spacing_string + f'{row.get("depreciated_enum_name", "")} = {id_value}{int_suffix} /* depreciated but identifier kept for backwards compatibility */'
        c_enum_content += (',\n' if id_value != sorted(c_enum_list)[-1] else '\n')

    c_enum_content += range_marker_render(c_range_marker)

    return c_enum_content

def update_c_typedef_enum(document_content, c_typedef_name, c_enum_name, c_head_comment, c_enum_list, c_range_marker = None, spacing_string = "  ", int_suffix = ""):
    def search_and_replace_c_typedef_enum(document_content, c_enum_content, typename, enumname = None):
        # Search and replace
        enumname = "" if enumname is None else (enumname + " ")
        pattern = fr'typedef enum [^{{]*\{{([^}}]*)\}} {typename};'
        replacement = f'typedef enum {enumname}{{\n{c_enum_content}}} {typename};'
        updated_document_content = re.sub(pattern, replacement, document_content, flags=re.DOTALL)
        return updated_document_content

    # Check if already exist, if not then create one
    if not get_content_of_typedef_enum(document_content, c_typedef_name):
        document_content += f'typedef enum {{\n}} {c_typedef_name};\n\n'

    # Old name takes priority for backwards compatibility (unless overridden)
    c_enum_content = override_enum_from_existing_typedef_enum(document_content, c_typedef_name, c_enum_list)

    # Generate enumeration header content
    c_enum_content = generate_c_enum_content(c_head_comment, c_enum_list, c_range_marker, spacing_string=spacing_string, int_suffix=int_suffix)

    # Search for typedef enum name and replace with new content
    updated_document_content = search_and_replace_c_typedef_enum(document_content, c_enum_content, c_typedef_name, c_enum_name)

    return updated_document_content

def get_content_of_const_macro(c_code: str, section_name: str) -> str:
    pattern = fr'\/\* Start of {section_name} autogenerated section \*\/(.*?)\/\* End of {section_name} autogenerated section \*\/'
    match = re.search(pattern, c_code, flags=re.DOTALL)
    if not match:
        return None

    return match.group(1)

def update_c_const_macro(document_content, section_name, c_head_comment, c_macro_list):
    def search_and_replace_c_const_macro(document_content, section_name, new_content):
        # Search and replace
        pattern = fr'\/\* Start of {section_name} autogenerated section \*\/(.*?)\n\/\* End of {section_name} autogenerated section \*\/'
        replacement = f'/* Start of {section_name} autogenerated section */\n{new_content}/* End of {section_name} autogenerated section */'
        updated_document_content = re.sub(pattern, replacement, document_content, flags=re.DOTALL)
        return updated_document_content

    # Check if already exist, if not then create one
    if not get_content_of_const_macro(document_content, section_name):
        document_content += f'/* Start of {section_name} autogenerated section */\n'
        document_content += f'/* End of {section_name} autogenerated section */'

    # Generate enumeration header content
    c_const_macro_content = c_head_comment
    for macro_name, macro_data in sorted(c_macro_list.items()):
        c_const_macro_content += f"#define {macro_name} {macro_data.get('value')}"
        if 'comment' in macro_data:
            c_const_macro_content += f" // {macro_data.get('comment')}"
        c_const_macro_content += "\n"

    # Search for typedef enum name and replace with new content
    updated_document_content = search_and_replace_c_const_macro(document_content, section_name, c_const_macro_content)

    return updated_document_content
