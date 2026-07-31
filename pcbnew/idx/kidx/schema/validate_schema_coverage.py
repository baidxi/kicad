#!/usr/bin/env python3
"""
IDX Schema Coverage Validation Script

Validates that the C++ implementation in kidx/schema/ covers all EDMD classes
defined in the ProSTEP EDMD XSD schema files.

Note: Schema uses camelCase (EDMDObject) while C++ uses underscores (EDMD_OBJECT).
This script handles the conversion between naming conventions.

Usage:
    python validate_schema_coverage.py
    python validate_schema_coverage.py --schema-dir /path/to/schemas
    python validate_schema_coverage.py --impl-dir /path/to/kidx/schema
    python validate_schema_coverage.py --missing-only
"""

import argparse
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple, Optional
from dataclasses import dataclass, field


# Default locations
DEFAULT_SCHEMA_DIR = "/home/fabien/Documents/kicad-dev/idx_ressources/PSI5_IDXv4.5_release/PSI5_IDXv4.5_Schema/PSI5_IDXv4.5_Schema"
DEFAULT_IMPL_DIR = "/home/fabien/Documents/kicad-dev/kicad-git/src/kicad-git/pcbnew/idx/kidx/schema"

# EDMD Schema files to check
SCHEMA_FILES = [
    "foundation.xsd",
    "geometry.d2.xsd",
    "pdm.xsd",
    "property.xsd",
    "annotation.xsd",
    "administration.xsd",
    "computational.xsd",
    "grouping.xsd",
    "external.xsd",
    "material.xsd",
    "text.xsd",
]


def camel_to_underscore(name: str) -> str:
    """Convert camelCase schema name to UPPER_CASE C++ name.
    
    Examples:
        EDMDObject -> EDMD_OBJECT
        EDMDDataSet -> EDMD_DATA_SET
        EDMDItemInstance -> EDMD_ITEM_INSTANCE
        EDMD3DModel -> EDMD_3D_MODEL
    """
    if not name.startswith("EDMD"):
        return name
    
    # Work with the part after EDMD
    rest = name[4:]
    
    # Special case: if rest starts with 3D or 2D, handle it specially
    if rest.startswith('3D'):
        rest = rest[2:]
        result = 'EDMD_3D'
    elif rest.startswith('2D'):
        rest = rest[2:]
        result = 'EDMD_2D'
    else:
        result = 'EDMD'
    
    # Insert underscore before any uppercase letter in rest
    processed_rest = re.sub(r'([A-Z])', r'_\1', rest)
    
    result = result + processed_rest
    
    # Collapse multiple underscores
    result = re.sub(r'_+', '_', result)
    # Remove trailing underscore if any
    result = result.rstrip('_')
    
    return result.upper()


def underscore_to_camel(name: str) -> str:
    """Convert UPPER_CASE C++ name to camelCase schema name.
    
    Examples:
        EDMD_OBJECT -> EDMDObject
        EDMD_DATA_SET -> EDMDDataSet
        EDMD_ITEM_INSTANCE -> EDMDItemInstance
        EDMD_3D_MODEL -> EDMD3DModel
    """
    if not name.startswith("EDMD_"):
        return name
    
    # Remove EDMD_ prefix temporarily
    rest = name[5:]
    
    # Split by underscores
    parts = rest.split("_")
    
    # Special handling for parts that are all numbers
    result_parts = []
    for part in parts:
        if part.isdigit():
            result_parts.append(part)
        else:
            result_parts.append(part.capitalize())
    
    return "EDMD" + "".join(result_parts)


@dataclass
class EDMDClass:
    """Represents an EDMD class from schema or implementation."""
    name: str  # In the native format (camelCase for schema, UPPER_CASE for impl)
    base_class: Optional[str] = None
    is_abstract: bool = False
    schema_file: Optional[str] = None
    impl_file: Optional[str] = None
    has_parse: bool = False
    has_write: bool = False
    has_resolve: bool = False
    members: List[str] = field(default_factory=list)
    parsed_members: Set[str] = field(default_factory=set)


@dataclass
class CoverageResult:
    """Result of coverage validation."""
    schema_classes: Set[str]  # In UPPER_CASE format
    impl_classes: Set[str]  # In UPPER_CASE format
    missing_classes: Set[str]
    extra_classes: Set[str]
    classes_without_parse: Set[str]
    classes_without_write: Set[str]
    classes_without_resolve: Set[str]
    total_schema: int = 0
    total_implemented: int = 0
    total_schema_members: int = 0
    total_implemented_members: int = 0
    coverage_percent: float = 0.0
    # Parse method coverage
    classes_with_missing_parse_members: Dict[str, Set[str]] = field(default_factory=dict)


class SchemaCoverageValidator:
    """Validates schema coverage in C++ implementation."""
    
    def __init__(self, schema_dir: str = DEFAULT_SCHEMA_DIR, impl_dir: str = DEFAULT_IMPL_DIR):
        self.schema_dir = Path(schema_dir)
        self.impl_dir = Path(impl_dir)
        self.schema_classes: Dict[str, EDMDClass] = {}  # Key: camelCase, Value: EDMDClass
        self.impl_classes: Dict[str, EDMDClass] = {}  # Key: UPPER_CASE, Value: EDMDClass
    
    def validate(self, check_methods: bool = True) -> CoverageResult:
        """Run full validation and return results."""
        # Extract classes from schema
        self._extract_schema_classes()
        
        # Extract classes from implementation
        self._extract_impl_classes(check_methods)
        
        # Calculate coverage
        return self._calculate_coverage()
    
    def _extract_schema_classes(self):
        """Extract EDMD_* classes from XSD schema files."""
        if not self.schema_dir.exists():
            print(f"Warning: Schema directory not found: {self.schema_dir}", file=sys.stderr)
            return
        
        for schema_file in SCHEMA_FILES:
            schema_path = self.schema_dir / schema_file
            if not schema_path.exists():
                continue
            
            with open(schema_path, 'r') as f:
                content = f.read()
            
            self._parse_schema_content(content, schema_file)
    
    def _parse_schema_content(self, content: str, schema_file: str):
        """Parse XSD content to find EDMD_* complexTypes and elements."""
        # First, collect all simpleType names to exclude them
        simple_type_pattern = re.compile(r'<xs:simpleType\s+name="(EDMD[^"]+)"')
        simple_types = set()
        for match in simple_type_pattern.finditer(content):
            simple_types.add(match.group(1))
        
        # Build a hierarchy map: class_name -> (base_class, [members])
        # First pass: find all complexTypes and their base classes
        hierarchy: Dict[str, Tuple[str, List[str]]] = {}
        
        # Find complexType definitions with name attribute
        # Pattern: <xs:complexType name="EDMD...">
        complex_type_pattern = re.compile(r'<xs:complexType\s+name="(EDMD[^"]+)"')
        for match in complex_type_pattern.finditer(content):
            class_name = match.group(1)
            
            # Check if abstract
            end_pos = match.end()
            snippet = content[end_pos:end_pos+100]
            is_abstract = 'abstract="true"' in snippet
            
            # Find base class - search in the type_content for extension base
            base_class = None
            # First check the snippet for abstract attribute
            base_pattern = re.compile(r'base="([^"]+)"')
            base_match = base_pattern.search(snippet)
            if base_match:
                base_name = base_match.group(1)
                if ':' in base_name:
                    base_name = base_name.split(':')[1]
                if base_name.startswith('EDMD'):
                    base_class = base_name
            
            # Extract members (elements) for this class
            # Find the complexType block
            type_start = match.start()
            # Find the end of the opening tag
            tag_end = content.find('>', type_start)
            if tag_end == -1:
                tag_end = type_start
            # Find the closing tag (handle variable whitespace)
            depth = 1
            type_end = type_start
            for i in range(tag_end, len(content)):
                if content[i:i+13] == '<xs:complexType':
                    depth += 1
                elif content[i:i+20].strip() == '</xs:complexType>':
                    depth -= 1
                    if depth == 0:
                        # Find the end of the closing tag
                        tag_close_end = content.find('>', i)
                        if tag_close_end == -1:
                            type_end = i + 15
                        else:
                            type_end = tag_close_end + 1
                        break
            
            type_content = content[type_start:type_end]
            
            # Find all element names in this complexType (including nested in extension/sequence)
            members: List[str] = []
            element_pattern = re.compile(r'<xs:element\s+[^>]*name="([^"]+)"')
            for elem_match in element_pattern.finditer(type_content):
                member_name = elem_match.group(1)
                members.append(member_name)
            
            # Also search for base in the type_content (for extension)
            if not base_class:
                base_match_in_content = base_pattern.search(type_content)
                if base_match_in_content:
                    base_name = base_match_in_content.group(1)
                    if ':' in base_name:
                        base_name = base_name.split(':')[1]
                    if base_name.startswith('EDMD'):
                        base_class = base_name
            
            hierarchy[class_name] = (base_class, members)
            
            self.schema_classes[class_name] = EDMDClass(
                name=class_name,
                schema_file=schema_file,
                is_abstract=is_abstract,
                members=members,
                base_class=base_class
            )
        
        # Find element definitions that might reference types
        # Pattern: <xs:element name="..." type="tns:EDMD..." />
        # or <xs:element name="..." type="EDMD..." />
        element_pattern = re.compile(r'<xs:element\s+[^>]*type="[^"]*:(EDMD[^"]+)"')
        for match in element_pattern.finditer(content):
            class_name = match.group(1)
            # Only add if not already present from complexType and not a simpleType
            if class_name not in self.schema_classes and class_name not in simple_types:
                self.schema_classes[class_name] = EDMDClass(
                    name=class_name,
                    schema_file=schema_file,
                    members=[]
                )
        
        # Second pass: recursively inherit members from base classes
        def get_all_members(class_name: str, visited: Set[str] = None) -> List[str]:
            if visited is None:
                visited = set()
            if class_name in visited:
                return []
            visited.add(class_name)
            
            if class_name not in hierarchy:
                return []
            
            base_class, members = hierarchy[class_name]
            if base_class and base_class in hierarchy:
                base_members = get_all_members(base_class, visited)
                return base_members + members
            return members
        
        for class_name in hierarchy:
            all_members = get_all_members(class_name)
            self.schema_classes[class_name].members = all_members
    
    def _extract_impl_classes(self, check_methods: bool):
        """Extract EDMD_* classes from C++ header and implementation files."""
        if not self.impl_dir.exists():
            print(f"Warning: Implementation directory not found: {self.impl_dir}", file=sys.stderr)
            return
        
        # Process all .h and .cpp files
        for file_path in sorted(self.impl_dir.glob("*.h")):
            self._parse_header_file(file_path, check_methods)
        
        for file_path in sorted(self.impl_dir.glob("*.cpp")):
            self._parse_impl_file(file_path, check_methods)
    
    def _parse_header_file(self, file_path: Path, check_methods: bool):
        """Parse a C++ header file for EDMD_* class declarations."""
        with open(file_path, 'r') as f:
            content = f.read()
        
        # Find class declarations: class EDMD_... : public EDMD_...
        class_pattern = re.compile(
            r'class\s+(EDMD_\w+)\s*:\s*(?:public\s+)?(?:EDMD_(\w+)\s*)?'
        )
        
        for match in class_pattern.finditer(content):
            class_name = match.group(1)
            base_class = match.group(2) if match.group(2) else None
            
            if class_name not in self.impl_classes:
                self.impl_classes[class_name] = EDMDClass(
                    name=class_name,
                    base_class=base_class,
                    impl_file=str(file_path.name)
                )
            else:
                # Update base class if found
                if base_class:
                    self.impl_classes[class_name].base_class = base_class
        
        # Check for Parse/Write/Resolve method declarations in header
        if check_methods:
            self._check_methods_in_content(content, file_path)
    
    def _parse_impl_file(self, file_path: Path, check_methods: bool):
        """Parse a C++ implementation file for EDMD_* class method definitions."""
        with open(file_path, 'r') as f:
            content = f.read()
        
        if check_methods:
            self._check_methods_in_content(content, file_path)
    
    def _check_methods_in_content(self, content: str, file_path: Path):
        """Check for Parse/Write/Resolve methods in the content."""
        # Find all method definitions
        # Pattern: bool EDMD_ClassName::Parse(IDX_XML_NODE& aNode)
        parse_pattern = re.compile(
            r'bool\s+(EDMD_\w+)::Parse\s*\(\s*IDX_XML_NODE'
        )
        write_pattern = re.compile(
            r'void\s+(EDMD_\w+)::Write\s*\(\s*IDX_XML_NODE'
        )
        resolve_pattern = re.compile(
            r'bool\s+(EDMD_\w+)::Resolve\s*\(\s*IDX_REGISTRY'
        )
        
        # Also check for virtual declarations in class definition
        # Pattern: virtual bool Parse(IDX_XML_NODE& aNode) override;
        virt_parse_pattern = re.compile(
            r'virtual\s+bool\s+Parse\s*\(\s*IDX_XML_NODE&'
        )
        virt_write_pattern = re.compile(
            r'virtual\s+void\s+Write\s*\(\s*IDX_XML_NODE&'
        )
        virt_resolve_pattern = re.compile(
            r'virtual\s+bool\s+Resolve\s*\(\s*IDX_REGISTRY&'
        )
        
        # Also extract parsed members from Parse() methods
        self._extract_parse_members(content, file_path)
        
        # For method definitions, we can directly map to class
        for match in parse_pattern.finditer(content):
            class_name = match.group(1)
            if class_name in self.impl_classes:
                self.impl_classes[class_name].has_parse = True
        
        for match in write_pattern.finditer(content):
            class_name = match.group(1)
            if class_name in self.impl_classes:
                self.impl_classes[class_name].has_write = True
        
        for match in resolve_pattern.finditer(content):
            class_name = match.group(1)
            if class_name in self.impl_classes:
                self.impl_classes[class_name].has_resolve = True
        
        # For virtual declarations, we need to find which class they belong to
        # Split content by class definitions
        class_blocks = re.split(r'(?=class\s+EDMD_\w+\s*:)', content)
        current_class = None
        
        for block in class_blocks:
            class_match = re.search(r'class\s+(EDMD_\w+)\s*:', block)
            if class_match:
                current_class = class_match.group(1)
            
            if current_class:
                if virt_parse_pattern.search(block):
                    if current_class in self.impl_classes:
                        self.impl_classes[current_class].has_parse = True
                if virt_write_pattern.search(block):
                    if current_class in self.impl_classes:
                        self.impl_classes[current_class].has_write = True
                if virt_resolve_pattern.search(block):
                    if current_class in self.impl_classes:
                        self.impl_classes[current_class].has_resolve = True
    
    def _extract_parse_members(self, content: str, file_path: Path):
        """Extract which members are parsed in Parse() methods."""
        # Find all Parse() method implementations
        # Pattern: bool EDMD_ClassName::Parse(IDX_XML_NODE& aNode)
        parse_method_pattern = re.compile(
            r'bool\s+(EDMD_\w+)\s*::\s*Parse\s*\(\s*IDX_XML_NODE&[^\)]*\)\s*\{([^\}]*)\}'
        )
        
        for match in parse_method_pattern.finditer(content):
            class_name = match.group(1)
            method_body = match.group(2)
            
            if class_name not in self.impl_classes:
                self.impl_classes[class_name] = EDMDClass(name=class_name, impl_file=str(file_path.name))
            
            # Extract all member names being parsed
            # Look for patterns like:
            #   if( IDX_keyword_compare( name, "d2:MemberName" ) )
            #   if( IDX_keyword_compare( name, "MemberName" ) )
            #   if( name == "MemberName" )
            #   m_member = ...
            
            parsed_members = set()
            
            # Pattern 1: IDX_keyword_compare with prefix
            pattern1 = re.compile(r'IDX_keyword_compare\s*\(\s*[^,]+\s*,\s*"([^"]+)"\s*\)')
            for m in pattern1.finditer(method_body):
                member_name = m.group(1)
                # Remove namespace prefix if present (e.g., "d2:Point" -> "Point")
                if ':' in member_name:
                    member_name = member_name.split(':')[1]
                parsed_members.add(member_name)
            
            # Pattern 2: direct string comparison
            pattern2 = re.compile(r'if\s*\(\s*[^)]*name\s*[=!]=?\s*"([^"]+)"')
            for m in pattern2.finditer(method_body):
                member_name = m.group(1)
                if ':' in member_name:
                    member_name = member_name.split(':')[1]
                parsed_members.add(member_name)
            
            # Pattern 3: member assignment (e.g., m_member = ...)
            pattern3 = re.compile(r'm_(\w+)\s*[=;]')
            for m in pattern3.finditer(method_body):
                member_name = m.group(1)
                parsed_members.add(member_name)
            
            # Store parsed members in the class info
            if not hasattr(self.impl_classes[class_name], 'parsed_members'):
                self.impl_classes[class_name].parsed_members = set()
            self.impl_classes[class_name].parsed_members.update(parsed_members)
    
    def _calculate_coverage(self) -> CoverageResult:
        """Calculate coverage statistics."""
        # Convert schema class names from camelCase to UPPER_CASE
        schema_class_names = set()
        for class_name in self.schema_classes.keys():
            converted = camel_to_underscore(class_name)
            schema_class_names.add(converted)
        
        # Implementation class names are already in UPPER_CASE
        impl_class_names = set(self.impl_classes.keys())
        
        missing = schema_class_names - impl_class_names
        extra = impl_class_names - schema_class_names
        
        classes_without_parse = set()
        classes_without_write = set()
        classes_without_resolve = set()
        classes_with_missing_parse_members: Dict[str, Set[str]] = {}
        
        for class_name, impl_class in self.impl_classes.items():
            if not impl_class.has_parse:
                classes_without_parse.add(class_name)
            if not impl_class.has_write:
                classes_without_write.add(class_name)
            if not impl_class.has_resolve:
                classes_without_resolve.add(class_name)
            
            # Check for missing Parse() members
            # Get the schema class name in camelCase
            schema_class_name = underscore_to_camel(class_name)
            if schema_class_name in self.schema_classes:
                schema_class = self.schema_classes[schema_class_name]
                if schema_class.members and impl_class.has_parse:
                    # Check which members are missing
                    missing_members = set()
                    for member in schema_class.members:
                        # Member names in schema are in camelCase or with namespaces
                        # Normalize to match what's in the C++ code
                        member_upper = camel_to_underscore(member).upper()
                        member_simple = member.split(':')[-1]  # Remove namespace
                        
                        # Check if this member is parsed
                        found = False
                        for parsed in impl_class.parsed_members:
                            # Try different comparisons
                            if (parsed.upper() == member_upper or 
                                parsed == member_simple or 
                                parsed.upper() == member_simple.upper() or
                                parsed == member):
                                found = True
                                break
                        
                        if not found:
                            missing_members.add(member)
                    
                    if missing_members:
                        classes_with_missing_parse_members[class_name] = missing_members
        
        total_schema = len(schema_class_names)
        # Only count implemented classes that are actually in the schema (exclude EXTRA)
        implemented_schema_classes = impl_class_names & schema_class_names
        total_impl = len(implemented_schema_classes)
        
        # Calculate member-based coverage
        total_schema_members = 0
        total_implemented_members = 0
        
        for schema_class_name, schema_class in self.schema_classes.items():
            # Convert to UPPER_CASE for comparison
            schema_class_upper = camel_to_underscore(schema_class_name)
            # Count total schema members
            total_schema_members += len(schema_class.members)
            
            # Check if this class is implemented
            if schema_class_upper in impl_class_names:
                impl_class = self.impl_classes[schema_class_upper]
                if impl_class.has_parse:
                    # Count implemented members
                    total_implemented_members += len(impl_class.parsed_members)
        
        # Use member count for coverage if there are members, otherwise fall back to class count
        if total_schema_members > 0:
            coverage = (total_implemented_members / total_schema_members * 100)
        else:
            coverage = (total_impl / total_schema * 100) if total_schema > 0 else 0.0
        
        return CoverageResult(
            schema_classes=schema_class_names,
            impl_classes=impl_class_names,
            missing_classes=missing,
            extra_classes=extra,
            classes_without_parse=classes_without_parse,
            classes_without_write=classes_without_write,
            classes_without_resolve=classes_without_resolve,
            total_schema=total_schema,
            total_implemented=total_impl,
            coverage_percent=coverage,
            classes_with_missing_parse_members=classes_with_missing_parse_members,
            total_schema_members=total_schema_members,
            total_implemented_members=total_implemented_members
        )


def print_coverage_result(result: CoverageResult, missing_only: bool = False, 
                          check_methods: bool = True):
    """Print the coverage results."""
    print("=" * 70)
    print("IDX Schema Coverage Report")
    print("=" * 70)
    print()
    
    print(f"Total schema classes: {result.total_schema}")
    print(f"Implemented classes:   {result.total_implemented}")
    print(f"Total schema members: {result.total_schema_members}")
    print(f"Implemented members:   {result.total_implemented_members}")
    print(f"Coverage:              {result.coverage_percent:.1f}%")
    print()
    
    if not missing_only:
        if result.missing_classes:
            print("-" * 70)
            print("MISSING CLASSES (in schema but not implemented):")
            print("-" * 70)
            for class_name in sorted(result.missing_classes):
                print(f"  - {class_name}")
            print()
        
        if result.extra_classes:
            print("-" * 70)
            print("EXTRA CLASSES (implemented but not in schema):")
            print("-" * 70)
            for class_name in sorted(result.extra_classes):
                print(f"  - {class_name}")
            print()
    else:
        # Only show missing
        if result.missing_classes:
            print("MISSING CLASSES:")
            for class_name in sorted(result.missing_classes):
                print(f"  - {class_name}")
        if result.extra_classes:
            print("\nEXTRA CLASSES:")
            for class_name in sorted(result.extra_classes):
                print(f"  - {class_name}")
    
    if check_methods:
        if result.classes_without_parse:
            print("-" * 70)
            print("CLASSES WITHOUT Parse() method:")
            print("-" * 70)
            for class_name in sorted(result.classes_without_parse):
                print(f"  - {class_name}")
            print()
        
        if result.classes_without_write:
            print("-" * 70)
            print("CLASSES WITHOUT Write() method:")
            print("-" * 70)
            for class_name in sorted(result.classes_without_write):
                print(f"  - {class_name}")
            print()
        
        if result.classes_without_resolve:
            print("-" * 70)
            print("CLASSES WITHOUT Resolve() method:")
            print("-" * 70)
            for class_name in sorted(result.classes_without_resolve):
                print(f"  - {class_name}")
            print()
        
        if result.classes_with_missing_parse_members:
            print("-" * 70)
            print("CLASSES WITH MISSING MEMBERS IN Parse():")
            print("-" * 70)
            for class_name in sorted(result.classes_with_missing_parse_members.keys()):
                missing_members = result.classes_with_missing_parse_members[class_name]
                print(f"  - {class_name}:")
                for member in sorted(missing_members):
                    print(f"      * {member}")
            print()
    
    if not missing_only and not result.missing_classes and not result.extra_classes:
        if check_methods:
            if not result.classes_without_parse and not result.classes_without_write and not result.classes_without_resolve:
                print("All schema classes are implemented with Parse/Write/Resolve methods!")
        else:
            print("All schema classes are implemented!")
    
    print("=" * 70)


def main():
    parser = argparse.ArgumentParser(
        description="Validate that C++ implementation covers all EDMD schema classes",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python validate_schema_coverage.py
    python validate_schema_coverage.py --schema-dir /path/to/schemas
    python validate_schema_coverage.py --impl-dir /path/to/kidx/schema
    python validate_schema_coverage.py --missing-only
    python validate_schema_coverage.py --no-method-check
        """
    )
    
    parser.add_argument(
        "-s", "--schema-dir",
        type=str,
        default=DEFAULT_SCHEMA_DIR,
        help="Path to directory containing XSD schema files"
    )
    
    parser.add_argument(
        "-i", "--impl-dir",
        type=str,
        default=DEFAULT_IMPL_DIR,
        help="Path to kidx/schema implementation directory"
    )
    
    parser.add_argument(
        "-m", "--missing-only",
        action="store_true",
        help="Only show missing and extra classes (no method checks)"
    )
    
    parser.add_argument(
        "--no-method-check",
        action="store_true",
        help="Skip checking for Parse/Write/Resolve methods"
    )
    
    args = parser.parse_args()
    
    # Create validator
    validator = SchemaCoverageValidator(
        schema_dir=args.schema_dir,
        impl_dir=args.impl_dir
    )
    
    # Run validation
    check_methods = not args.no_method_check
    result = validator.validate(check_methods=check_methods)
    
    # Print results
    print_coverage_result(result, missing_only=args.missing_only, 
                          check_methods=check_methods)
    
    # Exit with error code if coverage is incomplete
    if result.missing_classes or result.classes_without_parse or \
       result.classes_without_write or result.classes_without_resolve:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == "__main__":
    main()
