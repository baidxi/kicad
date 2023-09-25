#ifndef ALLEGRO_FILE_H_
#define ALLEGRO_FILE_H_

#include <wx/mstream.h>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>

using namespace boost::interprocess;

class ALLEGRO_FILE
{
public:
    ALLEGRO_FILE( const wxString& aFilePath );
    ~ALLEGRO_FILE() = default;

    mapped_region region;
};

#endif // ALLEGRO_FILE_H_
