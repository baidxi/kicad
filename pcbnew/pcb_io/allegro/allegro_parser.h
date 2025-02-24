#ifndef ALLEGRO_PARSER_H_
#define ALLEGRO_PARSER_H_

#include <cstdarg>

#include <boost/version.hpp>
#if BOOST_VERSION >= 108100
#include <boost/unordered/unordered_flat_map.hpp>
#else
#include <unordered_map>
#endif
#include <optional>
#include <variant>

// Includes for `ntohl`, which is used for displaying ints in the same format
// as a hex editor.
#ifdef __WINDOWS__
#include <winsock.h>
#else
#include <arpa/inet.h>
#endif

#include <wx/log.h>

#include <base_units.h>
#include <board.h>
#include <board_design_settings.h>
#include <geometry/eda_angle.h>
#include <footprint.h>
#include <layer_ids.h>
#include <lib_id.h>
#include <pad.h>
#include <pcb_shape.h>
#include <pcb_track.h>
#include <trigo.h>
#include <zone.h>

#include "allegro_file.h"
#include "allegro_structs.h"

#define PRINT_ALL_ITEMS 0

template <ALLEGRO::MAGIC magic>
class ALLEGRO_PARSER
{
public:
    ALLEGRO_PARSER( BOARD* aBoard, const ALLEGRO_FILE& aAllegroBrdFile );
    void Parse();

private:
    void    BuildBoard();
    uint8_t GetCopperLayerCount();
    void    AddAnnotation( BOARD_ITEM_CONTAINER& aContainer, const ALLEGRO::T_14<magic>& i14 );
    void    AddFootprint( const ALLEGRO::T_2B<magic>& i2B );

    void AddPad( FOOTPRINT* fp, const ALLEGRO::T_32<magic>& i32 );

    void AddText( BOARD_ITEM_CONTAINER& aContainer, const ALLEGRO::T_30<magic>& i30 );
    void AddTrack( const ALLEGRO::T_1B<magic>& i1B, const ALLEGRO::T_05_TRACK<magic>& i05 );
    void AddVia( const ALLEGRO::T_33<magic>& i33 );
    void AddZone( BOARD_ITEM_CONTAINER& aContainer, const std::optional<ALLEGRO::T_1B<magic>>& i1B,
                  const ALLEGRO::T_28_ZONE<magic>& i28 );

    void                    Skip( std::size_t n );
    void                    Log( const char* fmt... );
    bool                    IsType( uint32_t k, uint8_t t );
    std::optional<wxString> StringLookup( uint32_t k );
    NETINFO_ITEM*           NetInfo( uint32_t k );
    NETINFO_ITEM*           NetInfo( const ALLEGRO::T_04<magic>& i04 );
    SHAPE_LINE_CHAIN        ShapeStartingAt( uint32_t* k );

    template <class SEGMENT_TYPE>
    void AddSegmentToChain( SHAPE_LINE_CHAIN& chain, SEGMENT_TYPE* segment, bool first_segment );

    PCB_LAYER_ID EtchLayerToKi( ALLEGRO::LAYER_INFO a_layer );
    double       Scale( double a_allegroValue );

    const ALLEGRO::T_36_08<magic>* FindFont( uint8_t k );

    constexpr uint32_t             GetPadComponentCount( const ALLEGRO::T_1C<magic>& i1C );
    constexpr ALLEGRO::t13<magic>* GetPadComponent( const ALLEGRO::T_1C<magic>& i1C, uint32_t idx );
    void                           SetPadShape( PAD& pad, const ALLEGRO::t13<magic>& it13 );

    std::optional<wxString> RefdesLookup( const ALLEGRO::T_2D<magic>& i2D );

    template <template <ALLEGRO::MAGIC> typename T>
    static uint32_t DefaultParser( ALLEGRO_PARSER& parser );

    static uint32_t Parse03( ALLEGRO_PARSER& parser );
    static uint32_t Parse1C( ALLEGRO_PARSER& parser );
    static uint32_t Parse1D( ALLEGRO_PARSER& parser );
    static uint32_t Parse1E( ALLEGRO_PARSER& parser );
    static uint32_t Parse1F( ALLEGRO_PARSER& parser );
    static uint32_t Parse21( ALLEGRO_PARSER& parser );
    static uint32_t Parse27( ALLEGRO_PARSER& parser );
    static uint32_t Parse2A( ALLEGRO_PARSER& parser );
    static uint32_t Parse31( ALLEGRO_PARSER& parser );
    static uint32_t Parse35( ALLEGRO_PARSER& parser );
    static uint32_t Parse36( ALLEGRO_PARSER& parser );
    static uint32_t Parse3B( ALLEGRO_PARSER& parser );
    static uint32_t Parse3C( ALLEGRO_PARSER& parser );

    typedef uint32_t ( *PARSER_FUNC )( ALLEGRO_PARSER& parser );

    static constexpr PARSER_FUNC PARSER_TABLE[] = {
        // 0x00
        nullptr,
        // 0x01
        &DefaultParser<ALLEGRO::T_01_ARC>,
        // 0x02
        nullptr,
        // 0x03
        &Parse03,
        // 0x04
        &DefaultParser<ALLEGRO::T_04>,
        // 0x05
        &DefaultParser<ALLEGRO::T_05_TRACK>,
        // 0x06
        &DefaultParser<ALLEGRO::T_06>,
        // 0x07
        &DefaultParser<ALLEGRO::T_07>,
        // 0x08
        &DefaultParser<ALLEGRO::T_08>,
        // 0x09
        &DefaultParser<ALLEGRO::T_09>,
        // 0x0A
        &DefaultParser<ALLEGRO::T_0A>,
        // 0x0B
        nullptr,
        // 0x0C
        &DefaultParser<ALLEGRO::T_0C>,
        // 0x0D
        &DefaultParser<ALLEGRO::T_0D>,
        // 0x0E
        &DefaultParser<ALLEGRO::T_0E>,
        // 0x0F
        &DefaultParser<ALLEGRO::T_0F>,
        // 0x10
        &DefaultParser<ALLEGRO::T_10>,
        // 0x11
        &DefaultParser<ALLEGRO::T_11>,
        // 0x12
        &DefaultParser<ALLEGRO::T_12>,
        // 0x13
        nullptr,
        // 0x14
        &DefaultParser<ALLEGRO::T_14>,
        // 0x15
        &DefaultParser<ALLEGRO::T_15_SEGMENT>,
        // 0x16
        &DefaultParser<ALLEGRO::T_16_SEGMENT>,
        // 0x17
        &DefaultParser<ALLEGRO::T_17_SEGMENT>,
        // 0x18
        nullptr,
        // 0x19
        nullptr,
        // 0x1A
        nullptr,
        // 0x1B
        &DefaultParser<ALLEGRO::T_1B>,
        // 0x1C
        &Parse1C,
        // 0x1D
        &Parse1D,
        // 0x1E
        &Parse1E,
        // 0x1F
        &Parse1F,
        // 0x20
        &DefaultParser<ALLEGRO::T_20>,
        // 0x21
        &Parse21,
        // 0x22
        &DefaultParser<ALLEGRO::T_22>,
        // 0x23
        &DefaultParser<ALLEGRO::T_23>,
        // 0x24
        &DefaultParser<ALLEGRO::T_24>,
        // 0x25
        nullptr,
        // 0x26
        &DefaultParser<ALLEGRO::T_26>,
        // 0x27
        &Parse27,
        // 0x28
        &DefaultParser<ALLEGRO::T_28_ZONE>,
        // 0x29
        nullptr,
        // 0x2A
        &Parse2A,
        // 0x2B
        &DefaultParser<ALLEGRO::T_2B>,
        // 0x2C
        &DefaultParser<ALLEGRO::T_2C>,
        // 0x2D
        &DefaultParser<ALLEGRO::T_2D>,
        // 0x2E
        &DefaultParser<ALLEGRO::T_2E>,
        // 0x2F
        &DefaultParser<ALLEGRO::T_2F>,
        // 0x30
        &DefaultParser<ALLEGRO::T_30>,
        // 0x31
        &Parse31,
        // 0x32
        &DefaultParser<ALLEGRO::T_32>,
        // 0x33
        &DefaultParser<ALLEGRO::T_33>,
        // 0x34
        &DefaultParser<ALLEGRO::T_34>,
        // 0x35
        &Parse35,
        // 0x36
        &Parse36,
        // 0x37
        &DefaultParser<ALLEGRO::T_37>,
        // 0x38
        &DefaultParser<ALLEGRO::T_38_FILM>,
        // 0x39
        &DefaultParser<ALLEGRO::T_39_FILM_LAYER_LIST>,
        // 0x3A
        &DefaultParser<ALLEGRO::T_3A_FILM_LAYER_LIST_NODE>,
        // 0x3B
        &Parse3B,
        // 0x3C
        &Parse3C,
    };

    BOARD*              m_board;
    const ALLEGRO_FILE& m_brd;

    ALLEGRO::HEADER* m_header = nullptr;
    void*            m_baseAddr = nullptr;
    void*            m_curAddr = nullptr;

    double m_scaleFactor = 0;

    // 0 = not calculated yet
    uint16_t m_layerCount = 0;

    std::vector<std::tuple<uint32_t, uint32_t>> m_layers;
    std::map<uint32_t, char*>                   m_strings;
    std::map<uint32_t, ALLEGRO::T_2A>           m_t2A_map;
    std::map<uint32_t, ALLEGRO::T_1E>           m_t1E_map;
    std::map<uint32_t, ALLEGRO::T_36<magic>*>   m_t36_map;


#if BOOST_VERSION >= 108100
    boost::unordered_flat_map<uint32_t, void*> m_ptrs;
#else
    std::unordered_map<uint32_t, void*> m_ptrs;
#endif
};

#endif // ALLEGRO_PARSER_H_
