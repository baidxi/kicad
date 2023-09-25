#include "allegro_file.h"

ALLEGRO_FILE::ALLEGRO_FILE( const wxString& aFilePath )
{
    file_mapping mapped_file;
    mapped_file = file_mapping( aFilePath.c_str(), read_only );
    region = mapped_region( mapped_file, read_only );
};
