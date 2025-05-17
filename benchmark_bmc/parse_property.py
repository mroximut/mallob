import re
from typing import Dict, List, Optional, Tuple

def parse_property_file(property_file: str) -> Tuple[str, str, Optional[str]]:
    """
    Parse a property file and return the entry point, property type, and any additional options.
    
    Args:
        property_file: Path to the property file
        
    Returns:
        Tuple containing:
        - entry_point: The function name to check
        - property_type: The type of property being checked
        - additional_option: Optional additional parameter (like label name or fail function)
    """
    with open(property_file, 'r') as f:
        content = f.read().strip()
    
    # Remove whitespace
    content = re.sub(r'\s+', '', content)
    
    matches = []
    for line in content.split('CHECK'):
        print(line)
        m = re.search(r'\(init\((\S+)\(\)\),LTL\((\S+)\)\)', line)
        if m:
            matches.append((m.group(1), m.group(2)))
    if not matches:
        raise ValueError("Invalid property file format")
    
    entry_points = [match[0] for match in matches]
    ltl_properties = [match[1] for match in matches]
    
    # Determine property type and additional options
    property_types = []
    additional_options = []
    for ltl_property, entry_point in zip(ltl_properties, entry_points):
        additional_option = None
        if re.match(r'^G!label\((\S+)\)$', ltl_property):
            property_type = "label"
            additional_option = entry_point
        elif re.match(r'^G!call\((\S+)\(\)\)$', ltl_property):
            property_type = "unreach_call"
            additional_option = re.match(r'^G!call\((\S+)\(\)\)$', ltl_property).group(1)
        elif ltl_property == "Gassert":
            property_type = "unreach_call"
        elif re.match(r'^Gvalid-(free|deref|memtrack)$', ltl_property):
            property_type = "memsafety"
        elif ltl_property == "Gvalid-memcleanup":
            property_type = "memcleanup"
        elif ltl_property == "G!overflow":
            property_type = "overflow"
        elif ltl_property == "Fend":
            property_type = "termination"
        elif re.match(r'^G!uncaught\((\S+)\)$', ltl_property):
            property_type = "runtime-exception"
            additional_option = entry_point

        property_types.append(property_type)
        additional_options.append(additional_option)
    
    if not property_types:
        raise ValueError("Unrecognized property specification")
    
    return entry_points, property_types, additional_options

def get_property_options(property_types: List[str], additional_options: List[Optional[str]]) -> str:
    """
    Get the command line options for a given property type.
    
    Args:
        property_type: The type of property being checked
        additional_option: Optional additional parameter
        
    Returns:
        String containing the command line options
    """
    property_options = {
        "label": ["--error-label"],
        "unreach_call": [""],
        "termination": ["--no-assertions", "--no-self-loops-to-assumptions"],
        "overflow": ["--signed-overflow-check", "--no-assertions"],
        "memsafety": ["--pointer-check", "--memory-leak-check", "--bounds-check", "--no-assertions"],
        "memcleanup": ["--pointer-check", "--memory-leak-check", "--memory-cleanup-check", "--bounds-check", "--no-assertions"],
        "runtime-exception": ["--uncaught-exception-check-only-for"]
    }

    # Remove duplicates while preserving order
    seen = set()
    unique_property_types = []
    unique_additional_options = []
    for prop_type, add_opt in zip(property_types, additional_options):
        if prop_type not in seen:
            seen.add(prop_type)
            unique_property_types.append(prop_type)
            unique_additional_options.append(add_opt)

    assert(len(unique_property_types) == 1)
    options = []
    for property_type, additional_option in zip(unique_property_types, unique_additional_options):
        if property_type not in property_options:
            raise ValueError(f"Unknown property type: {property_type}")
        
        options.append(property_options[property_type])
        
        if property_type in ["label", "runtime-exception"] and additional_option:
            options.append(additional_option)
    
    return set(options) 

if __name__ == "__main__":
    # Print raw content of file
    with open("../../benchmark/sv-benchmarks/c/properties/valid-memsafety.prp", "r") as f:
        print(f.read())
    print("\n---\n")

    x, y, z = parse_property_file("../../benchmark/sv-benchmarks/c/properties/valid-memsafety.prp")
    print(x)
    print(y)
    print(z)

    print(get_property_options(y, z))
