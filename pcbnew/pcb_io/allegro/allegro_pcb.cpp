#include <wx/log.h>

#include "allegro_pcb.h"

#include "allegro_parser.h"

ALLEGRO_PCB::ALLEGRO_PCB( BOARD* aBoard ) : m_board( aBoard )
{
}

void ALLEGRO_PCB::Parse( const ALLEGRO_FILE& aAllegroBrdFile )
{
    // wxLogMessage( "p: %p\n", aAllegroBrdFile.region.get_address() );
    uint32_t magic = *(uint32_t*) ( aAllegroBrdFile.region.get_address() );
    switch( magic )
    {
    case 0x00130000:
    case 0x00130200:
    {
        ALLEGRO_PARSER<ALLEGRO::A_160>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00130402:
    {
        ALLEGRO_PARSER<ALLEGRO::A_162>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00130C03:
    {
        ALLEGRO_PARSER<ALLEGRO::A_164>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00131003:
    {
        ALLEGRO_PARSER<ALLEGRO::A_165>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00131503:
    case 0x00131504:
    {
        ALLEGRO_PARSER<ALLEGRO::A_166>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00140400:
    case 0x00140500:
    case 0x00140501:
    case 0x00140502:
    case 0x00140600:
    case 0x00140700:
    {
        ALLEGRO_PARSER<ALLEGRO::A_172>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00140900:
    case 0x00140901:
    case 0x00140902:
    case 0x00140E00:
    {
        ALLEGRO_PARSER<ALLEGRO::A_174>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    case 0x00141502:
    {
        ALLEGRO_PARSER<ALLEGRO::A_175>( m_board, aAllegroBrdFile ).Parse();
        return;
    }
    }
}
