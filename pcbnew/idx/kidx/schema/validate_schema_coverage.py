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
        EDMD_DATASET -> EDMDDataSet
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
    coverage_percent: float = 0.0


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
        # Find complexType definitions with name attribute
        # Pattern: <xs:complexType name="EDMD...">
        complex_type_pattern = re.compile(r'<xs:complexType\s+name="(EDMD[^"]+)"')
        for match in complex_type_pattern.finditer(content):
            class_name = match.group(1)
            
            # Check if abstract
            # Look ahead for abstract="true" in the next ~100 chars
            end_pos = match.end()
            snippet = content[end_pos:end_pos+100]
            is_abstract = 'abstract="true"' in snippet
            
            self.schema_classes[class_name] = EDMDClass(
                name=class_name,
                schema_file=schema_file,
                is_abstract=is_abstract
            )
        
        # Find element definitions that might reference types
        # Pattern: <xs:element name="..." type="tns:EDMD..." />
        # or <xs:element name="..." type="EDMD..." />
        element_pattern = re.compile(r'<xs:element\s+[^>]*type="[^"]*:(EDMD[^"]+)"')
        for match in element_pattern.finditer(content):
            class_name = match.group(1)
            # Only add if not already present from complexType
            if class_name not in self.schema_classes:
                self.schema_classes[class_name] = EDMDClass(
                    name=class_name,
                    schema_file=schema_file
                )
    
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
        
        for class_name, impl_class in self.impl_classes.items():
            if not impl_class.has_parse:
                classes_without_parse.add(class_name)
            if not impl_class.has_write:
                classes_without_write.add(class_name)
            if not impl_class.has_resolve:
                classes_without_resolve.add(class_name)
        
        total_schema = len(schema_class_names)
        total_impl = len(impl_class_names)
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
            coverage_percent=coverage
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
