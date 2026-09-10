# IDX Schema Coverage Validation

This script validates that the C++ implementation in `kidx/schema/` covers all EDMD classes defined in the ProSTEP EDMD XSD schema files.

## What It Checks

1. **Class Coverage**: Reports which schema classes are missing from the C++ implementation
2. **Method Coverage**: Reports which implemented classes are missing Parse/Write/Resolve methods
3. **Naming Convention**: Handles the conversion between schema camelCase (EDMDObject) and C++ UPPER_CASE (EDMD_OBJECT)

## Dependencies

- Python 3.x (standard library only, no external packages required)

## Usage

```bash
# Run full validation (class coverage + method check)
python validate_schema_coverage.py

# Only show missing classes (faster)
python validate_schema_coverage.py --missing-only

# Skip method checking (only check class existence)
python validate_schema_coverage.py --no-method-check

# Use custom schema directory
python validate_schema_coverage.py --schema-dir /path/to/schemas

# Use custom implementation directory
python validate_schema_coverage.py --impl-dir /path/to/kidx/schema
```

## Output

The script outputs:
- Total number of schema classes
- Number of implemented classes
- Coverage percentage
- List of missing classes (in schema but not implemented)
- List of extra classes (implemented but not in schema)
- List of classes without Parse() method
- List of classes without Write() method
- List of classes without Resolve() method

## Exit Codes

- `0`: All checks passed (100% coverage with all methods)
- `1`: Missing classes or methods detected

## Default Paths

- Schema directory: `/home/fabien/Documents/kicad-dev/idx_ressources/PSI5_IDXv4.5_release/PSI5_IDXv4.5_Schema/PSI5_IDXv4.5_Schema/`
- Implementation directory: `pcbnew/idx/kidx/schema/` (relative to script location)

## Schema Files Checked

- foundation.xsd
- geometry.d2.xsd
- pdm.xsd
- property.xsd
- annotation.xsd
- administration.xsd
- computational.xsd
- grouping.xsd
- external.xsd
- material.xsd
- text.xsd

## Known Limitations

1. **3D Conversion**: Schema classes like `EDMD3DModel` are converted to `EDMD_3_D_MODEL` instead of `EDMD_3D_MODEL`. The C++ implementation uses `EDMD_3D_MODEL`. This is a known limitation of the camelCase to UPPER_CASE conversion.

2. **Inheritance**: The script does not validate inheritance hierarchies (i.e., whether C++ classes inherit from the correct base classes as defined in the schema).

3. **Members**: The script does not validate that all class members are implemented and correctly parsed.

4. **Abstract Classes**: Abstract schema classes may not need C++ implementations if they're not instantiated directly.

## Integration

The script can be integrated into a CI/CD pipeline to ensure schema coverage doesn't regress:

```bash
# In your test suite
python pcbnew/idx/kidx/schema/validate_schema_coverage.py --missing-only || echo "Schema coverage incomplete"
```
