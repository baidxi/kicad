#include <wx/wfstream.h>
#include <wx/log.h>
#include <wx/translation.h>

#include <board.h>

#include "allegro_file.h"
#include "allegro_pcb.h"
#include "allegro_structs.h"
#include "pcb_io_allegro.h"

PCB_IO_ALLEGRO::PCB_IO_ALLEGRO() : PCB_IO( wxS( "Cadence Allegro" ) ), m_board( nullptr ) {};

PCB_IO_ALLEGRO::~PCB_IO_ALLEGRO() = default;

bool PCB_IO_ALLEGRO::CanReadBoard( const wxString& aFileName ) const
{
    if( !PCB_IO::CanReadBoard( aFileName ) )
        return false;

    wxFileInputStream input( aFileName );
    if( input.IsOk() && !input.Eof() )
    {
        uint32_t magic = 0;
        if( input.GetLength() < sizeof( magic ) )
        {
            return false;
        }

        if( !input.ReadAll( &magic, sizeof( magic ) ) )
        {
            return false;
        }

        if( ALLEGRO::A_160 <= magic && magic <= ALLEGRO::A_MAX )
        {
            return true;
        }
    }

    return false;
};

bool PCB_IO_ALLEGRO::CanReadFootprint( const wxString& aFileName ) const
{
    wxLogWarning( "Func %s", __FUNCTION__ );
    return CanReadBoard( aFileName );
};

bool PCB_IO_ALLEGRO::CanReadFootprintLib( const wxString& aFileName ) const
{
    wxLogWarning( "Func %s", __FUNCTION__ );
    return CanReadBoard( aFileName );
};

BOARD* PCB_IO_ALLEGRO::LoadBoard( const wxString& aFileName, BOARD* aAppendToMe,
                                  const std::map<std::string, UTF8>* aProperties,
                                  PROJECT*                           aProject )
{
    // FIXME: What is the intent of appending to a board?
    m_board = aAppendToMe ? aAppendToMe : new BOARD();

    if( !aAppendToMe )
    {
        m_board->SetFileName( aFileName );
    }

    ALLEGRO_FILE allegroBrdFile( aFileName );
    ALLEGRO_PCB  pcb( m_board );
    pcb.Parse( allegroBrdFile );

    // this->m_layer_mapping_handler( std::vector<INPUT_LAYER_DESC>() );

    return m_board;
}
