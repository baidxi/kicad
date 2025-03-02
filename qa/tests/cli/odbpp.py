#
# This program source code file is part of KiCad, a free EDA CAD application.
#
# Copyright (C) 2025 KiCad Developers
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#
import logging
import re
from pathlib import Path

logger = logging.getLogger("cli_odbpp")

def is_date(string):
    pattern = r"^# \d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}$"
    return bool(re.match(pattern, string))


def contains_version_header(line):
    return line.startswith("HDR KiCad EDA")


def is_float(value):
    try:
        float_value = float(value)
        return not float_value.is_integer()
    except ValueError:
        return False


def compare_lines(line1, line2, allowed_deviation):
    """Compare two lines word by word.
        Ignore lines with a date.
        Ignore lines with a KiCad version header.
        Allow certain deviation for floating point values.
    """

    if is_date(line1) and is_date(line2):
        logger.debug(f"ignoring line '{line1}'")
        return True

    if contains_version_header( line1 ) and contains_version_header( line2 ):
        logger.debug( f"ignoring line '{line1}'" )
        return True

    words1 = line1.split()
    words2 = line2.split()

    if len(words1) != len(words2):
        return False

    for word1, word2 in zip(words1, words2):

        if is_float(word1) and is_float(word2):

            difference = abs( float( word1 ) - float( word2 ))

            if difference > allowed_deviation:
                return False

        elif word1 != word2:
            return False

    return True


def compare_files(file1_path, file2_path, allowed_deviation, allowed_swaps = 2):
    """ Compare the content of two files.
        Dates and KiCad version headers are ignored.
        floating values can have a certain deviation.
        lines can be swapped in the features section, but not the header section.
    """
    logger.debug( file1_path )
    unmatched_lines_a = []
    unmatched_lines_b = []

    in_header = True

    content_begin = ["#Feature attribute text strings",
                     "#Layer features"]

    with open(file1_path, 'r') as file1, open(file2_path, 'r') as file2:
        while True:
            line1 = file1.readline()
            line2 = file2.readline()

            if not line1 and not line2:
                break

            line1 = line1.strip() if line1 else None
            line2 = line2.strip() if line2 else None

            if line1 is None:
                logger.error(f"unexpected EOF in file {file1}")
                return False

            if line2 is None:
                logger.error(f"unexpected EOF in file {file2}")
                return False

            if not compare_lines(line1, line2, allowed_deviation):
                if in_header:
                    logger.error(f"{line1} != {line2}")
                    return False
                # reordering of lines is allowed in content section.
                unmatched_lines_a.append( line1 )
                unmatched_lines_b.append( line2 )

            if line1 in content_begin:
                in_header = False

    if len(unmatched_lines_a) == 0:
        return True

    num_swaps = 0

    for line1, line2 in zip(sorted(unmatched_lines_a), sorted(unmatched_lines_b)):
        if not compare_lines(line1, line2, allowed_deviation):
            logger.error(f"{line1} != {line2}")
            return False

        num_swaps += 1
        logger.warning( f"{line1} was swapped." )

    return (num_swaps / 2) < allowed_swaps


class ODBPP_Matrix:
    def __init__(self):
        self.step = {}
        self.layers = {}


    def clear(self):
        self.step = {}
        self.layers = {}


    def add_layer(self, data):
        """Adds a layer to the matrix.
        """

        layer_name = data["NAME"]

        if self.layers is not None and layer_name in self.layers:
            logger.error(f"Duplicate layer name {layer_name}")
            return False

        self.layers[layer_name] = data

        return True


    def is_equal(self, other):
        """Checks this matrix for equality with a different matrix file.
            The order of layers does not matter.
        """

        if self.step != other.step:
            return False

        for layer_ours,layer_theirs in zip(sorted(self.layers), sorted(other.layers)):

            if layer_ours != layer_theirs:
                return False

        return True


class ODBPP_Files:
    def __init__(self, basepath):
        self.reset(basepath)


    def reset(self, basepath):
        self.basepath = Path(basepath)

        self.features = []
        self.components = []
        self.profile = self.basepath / "steps" / "pcb" / "profile"
        self.stephdr = self.basepath / "steps" / "pcb" / "stephdr"
        self.matrix = self.basepath / "matrix" / "matrix"
        self.eda_data = self.basepath / "steps" / "pcb" / "eda" / "data"

        self.layers_dir = self.basepath / "steps" / "pcb" / "layers"


    def all(self):
        """Get all files as one list.
        """
        return self.features + self.components + [self.profile, self.stephdr, self.matrix, self.eda_data]


    def add_feature(self, layer_name):
        """Add a features layer files.
            Some lines can be swapped in the features section, since this would generally not alter outputs.
        """
        self.features.append(self.layers_dir / layer_name.lower() / "features")


    def add_component(self, layer_name):
        """Add a component layer file.
            They are distinct from feature files, since they should not allow any swapped lines.
        """
        self.features.append(self.layers_dir / layer_name.lower() / "components")


    def is_equal(self, other, allowed_deviation = 0.01):
        """Checks whether the set of files is equal to another set of files.
            Checks for the correct number of files, and the contents of feature files.
            A certain deviation is allowed, some files can have a certain number of swapped lines in the features section
            to still be counted as equal.
        """

        if len(self.features) != len(other.features):
            return False

        if len(self.components) != len(other.components):
            return False

        if not compare_files( self.profile, other.profile, allowed_deviation, 4 ):
            return False

        if not compare_files( self.stephdr, other.stephdr, allowed_deviation, 0 ):
            return False

        if not compare_files( self.eda_data, other.eda_data, allowed_deviation, 0 ):
            return False

        for ours, theirs in zip( self.components, other.components ):
            if not compare_files( ours, theirs, allowed_deviation, 0 ):
                return False

        for ours, theirs in zip( self.features, other.features ):
            if not compare_files( ours, theirs, allowed_deviation, 4 ):
                return False

        return True



class ODBPP:
    def __init__(self, basepath):
        self.basepath = Path(basepath)
        self.matrix = ODBPP_Matrix()
        self.files = ODBPP_Files( basepath )


    def read_matrix(self):
        """Read the matrix file.
            This function does rudimentary checks on the matrix file, like double layers etc.
        """

        if not self.files.matrix.exists():
            logger.error(f"Matrix file does not exist: {self.files.matrix}")
            return False

        self.matrix.clear()

        current_type = None
        current_fields = {}
        step_at_line = -1

        with open(self.files.matrix, "r") as f:
            current_line = 0

            for line in f:
                current_line = current_line + 1

                line = line.strip()

                if current_type is None:

                    if line.startswith("STEP {"):
                        if step_at_line > 0:
                            logger.error(f"Unexpected STEP section at {current_line}. First STEP section at {step_at_line}.")
                            return False

                        current_type = "STEP"

                    elif line.startswith("LAYER {"):
                        current_type = "LAYER"

                elif line.startswith("}"):
                    if current_type == "STEP":
                        self.matrix.step = current_fields

                    else:
                        if "NAME" not in current_fields:
                            logger.error(f"No Name defined for layer in matrix on line {current_line}")
                            return False

                        if not self.matrix.add_layer( current_fields ):
                            logger.error(f"Duplicate layer name found on line {current_line}")
                            return False

                    current_type = None
                    current_fields = {}

                else:
                    key, value = map(str.strip, line.split("=", 1))
                    current_fields[key] = value

        if current_type is not None:
            logger.error("Unexpected EOF.")
            return False


    def build_file_list(self):
        """Build the list of files included in the ODB++ export.
        """

        self.files.reset(self.basepath)

        if self.matrix.step is None or self.matrix.layers is None:
            if not self.files.matrix.exists():
                logger.error(f"Matrix file does not exist: {self.matrix_file}")
                return False

            self.read_matrix()

        for layer_name, layer_data in self.matrix.layers.items():
            self.files.add_feature( layer_name )

            if layer_data["TYPE"] == "COMPONENT":
                self.files.add_component( layer_name )


    def check_files_exist(self):
        """Checks if all files in the ODB++ directory structure exist.
        """
        missing_files = []

        for file_path in self.files.all():
            if not file_path.exists():
                missing_files.append(file_path)

        if missing_files:
            logger.warning("The following files are missing: %s", ", ".join(str(f) for f in missing_files))
            return False

        logger.info("All required files exist.")
        return True


    def is_equal(self, other):
        """Checks if this ODB++ export is equal to a differen ODB++ export.
        """
        if not self.matrix.is_equal( other.matrix ):
            return False

        if not self.files.is_equal( other.files ):
            return False

        return True


def compare_exports(location_under_test, reference_location):
    """Compare an ODB++ export to a reference ODB++ export.

        This does not validate the reference ODB++ export.
    """

    reference = ODBPP( reference_location )

    reference.read_matrix()

    reference.build_file_list()

    reference.check_files_exist()


    lut = ODBPP( location_under_test )

    if not lut.read_matrix():
        return False

    if not lut.build_file_list():
        return False

    if not lut.check_files_exist():
        return False

    return reference.is_equal( lut )


