#include "allegro_file.h"

#include "ki_exception.h"
#include <wx/translation.h>

ALLEGRO_FILE::ALLEGRO_FILE( const wxString& aFilePath )
{
    file_mapping mapped_file;
    try
    {
        mapped_file = file_mapping( aFilePath.c_str(), read_only );
    }
    catch( interprocess_exception exception )
    {
        THROW_IO_ERROR( _( "Failed to open file." ) );
    }
    region = mapped_region( mapped_file, read_only );
};
