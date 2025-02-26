#include "allegro_parser.h"

#include <wx/string.h>

// Helper functions
uint32_t round_to_word( uint32_t len )
{
    if( len % 4 != 0 )
    {
        return len / 4 * 4 + 4;
    }
    else
    {
        return len;
    }
}

double cfp_to_double( ALLEGRO::CADENCE_FP r )
{
    ALLEGRO::CADENCE_FP swapped;
    swapped.x = r.y;
    swapped.y = r.x;
    double g;
    std::memcpy( &g, &swapped, 8 );
    return g;
}

template <ALLEGRO::MAGIC magic>
ALLEGRO_PARSER<magic>::ALLEGRO_PARSER( BOARD* aBoard, const ALLEGRO_FILE& aAllegroBrdFile ) :
        m_board( aBoard ), m_brd( aAllegroBrdFile ){};

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::Parse()
{
    m_baseAddr = m_brd.region.get_address();
    size_t size = m_brd.region.get_size();

    m_curAddr = m_baseAddr;

    m_header = static_cast<ALLEGRO::HEADER*>( m_curAddr );
    Skip( sizeof( ALLEGRO::HEADER ) );

    m_ptrs.reserve( m_header->object_count );

    // Cache a fixed scale factor
    switch( m_header->units )
    {
    case ALLEGRO::BRD_UNITS::IMPERIAL:
        m_scaleFactor = ( (double) 25400. ) / m_header->unit_divisor;
        break;
    case ALLEGRO::BRD_UNITS::METRIC:
        m_scaleFactor = ( (double) 1000000. ) / m_header->unit_divisor;
        break;
    default: wxLogError( "Unrecognized units type: 0x%02X", m_header->units ); return;
    };

    // Layer map
    for( uint8_t i = 0; i < 25; i++ )
    {
        uint32_t xs[2] = { *( (uint32_t*) m_curAddr ), *( ( (uint32_t*) m_curAddr ) + 1 ) };
        Skip( sizeof( xs ) );
        m_layers.emplace_back( xs[0], xs[1] );
    }

    // Strings map
    m_curAddr = (char*) m_baseAddr + 0x1200;
    for( uint32_t i = 0; i < m_header->strings_count; i++ )
    {
        uint32_t id = *( (uint32_t*) ( m_curAddr ) );
        Skip( sizeof( id ) );

        m_strings[id] = (char*) m_curAddr;

        // Add one to include the NULL byte that might force the length to one
        // word longer.
        uint32_t len = strlen( (char*) m_curAddr );
        Skip( round_to_word( len + 1 ) );

        // wxLogMessage( "%08X = %s\n", id, m_strings[id] );
    }


    // All other objects
    while( m_curAddr < (char*) m_baseAddr + size && *static_cast<uint8_t*>( m_curAddr ) != 0x00 )
    {
        uint8_t t = *static_cast<uint8_t*>( m_curAddr );
        if( PRINT_ALL_ITEMS )
        {
            Log( "Handling t=0x%02X: ", t );
        }

        const PARSER_FUNC parser = PARSER_TABLE[t];
        if( t < sizeof( PARSER_TABLE ) / sizeof( PARSER_FUNC ) && parser != nullptr )
        {
            parser( *this );
        }
        else
        {
            THROW_IO_ERROR(
                    wxString::Format( "Do not have parser for t=0x%02X available.", ntohl( t ) ) );
        }
    }

    BuildBoard();
}

template <ALLEGRO::MAGIC magic>
template <template <ALLEGRO::MAGIC> typename T>
uint32_t ALLEGRO_PARSER<magic>::DefaultParser( ALLEGRO_PARSER<magic>& parser )
{
    auto* inst = static_cast<T<magic>*>( parser.m_curAddr );
    parser.m_ptrs[inst->k] = parser.m_curAddr;
    size_t size = ALLEGRO::sizeof_allegro_obj<T<magic>>();
    parser.Skip( size );
    return inst->k;
};

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse03( ALLEGRO_PARSER<magic>& parser )
{
    auto*    i = static_cast<ALLEGRO::T_03<magic>*>( parser.m_curAddr );
    uint32_t k = ALLEGRO_PARSER<magic>::DefaultParser<ALLEGRO::T_03>( parser );

    char* buf;
    // inst.has_str = false;
    uint32_t size;

    // log(base_addr_glb, address, "- Subtype.t = 0x%02X\n", i->subtype.t);
    switch( i->subtype.t & 0xFF )
    {
    case 0x65: break;
    case 0x64:
    case 0x66:
    case 0x67:
    case 0x6A: parser.Skip( 4 ); break;
    case 0x6D:
    case 0x6E:
    case 0x6F:
    case 0x68:
    case 0x6B:
    case 0x71:
    case 0x73:
    case 0x78: parser.Skip( round_to_word( i->subtype.size ) ); break;
    case 0x69: parser.Skip( 8 ); break;
    case 0x6C:
        size = *( static_cast<uint32_t*>( (void*) parser.m_curAddr ) );
        parser.Skip( 4 + 4 * size );
        break;
    case 0x70:
    case 0x74:
        uint16_t x[2];
        x[0] = *( static_cast<uint16_t*>( (void*) parser.m_curAddr ) );
        parser.Skip( 2 );
        x[1] = *( static_cast<uint16_t*>( (void*) parser.m_curAddr ) );
        parser.Skip( 2 );
        parser.Skip( x[1] + 4 * x[0] );
        break;
    case 0xF6: parser.Skip( 80 ); break;
    default:
        // log( fs.region.get_address(), address, "- Unexpected value subtype=0x%02X\n",
        //      i->subtype.t );
        parser.Log( "Unexpected value subtype = 0x%02X", i->subtype.t );
        exit( 1 );
    };

    return 0;
};

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse1C( ALLEGRO_PARSER<magic>& parser )
{
    auto*    i = static_cast<ALLEGRO::T_1C<magic>*>( parser.m_curAddr );
    uint32_t k = ALLEGRO_PARSER<magic>::DefaultParser<ALLEGRO::T_1C>( parser );

    uint16_t size = 0;
    if constexpr( magic < ALLEGRO::A_172 )
    {
        // printf("layer count 1 %d\n", i->layer_count);
        size = 10 + i->layer_count * 3;
    }
    else
    {
        // printf("layer count 2 %d\n", i->layer_count);
        size = 21 + i->layer_count * 4;
    }

    for( uint32_t i = 0; i < size; i++ )
    {
        // log(fs.region.get_address(), address, "- Skipping t13 %d\n", i);
        // log_n_words(address, 4);
        parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::t13<magic>>() );
    }
    // skip(address, size * sizeof_until_tail<t13<magic>>());

    if constexpr( magic >= ALLEGRO::A_172 )
    {
        parser.Skip( i->n * 40 );
    }
    else
    {
        parser.Skip( i->n * 32 - 4 );
    }

    return k;
};

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse1D( ALLEGRO_PARSER<magic>& parser )
{
    auto*    i = static_cast<ALLEGRO::T_1D<magic>*>( parser.m_curAddr );
    uint32_t k = ALLEGRO_PARSER<magic>::DefaultParser<ALLEGRO::T_1D>( parser );

    // log(&f, "size_a = %d, size_b = %d\n", size_a, size_b);
    parser.Skip( i->size_b * ( magic >= ALLEGRO::A_162 ? 56 : 48 ) );
    parser.Skip( i->size_a * 256 );
    if( magic >= ALLEGRO::A_172 )
    {
        parser.Skip( 4 );
    }
    return 0;
};

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse1E( ALLEGRO_PARSER<magic>& parser )
{
    ALLEGRO::T_1E* x1E_inst = new ALLEGRO::T_1E;
    memcpy( x1E_inst, parser.m_curAddr, sizeof( ALLEGRO::T_1E_HEADER ) );
    parser.Skip( sizeof( ALLEGRO::T_1E_HEADER ) );
    // f.read((char*)x1E_inst, sizeof(x1E_hdr));
    x1E_inst->s = (char*) parser.m_curAddr;
    parser.Skip( round_to_word( x1E_inst->hdr.size ) );
    // f.read(x1E_inst->s, round_to_word(x1E_inst->hdr.size));
    parser.m_t1E_map[x1E_inst->hdr.k] = *x1E_inst;
    if( magic >= ALLEGRO::A_172 )
    {
        parser.Skip( 4 );
    }
    return 0;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse1F( ALLEGRO_PARSER<magic>& parser )
{
    auto*    i = static_cast<ALLEGRO::T_1F<magic>*>( parser.m_curAddr );
    uint32_t k = ALLEGRO_PARSER<magic>::DefaultParser<ALLEGRO::T_1F>( parser );

    if constexpr( magic >= ALLEGRO::A_175 )
    {
        parser.Skip( i->size * 384 + 8 );
    }
    else if constexpr( magic >= ALLEGRO::A_172 )
    {
        parser.Skip( i->size * 280 + 8 );
    }
    else if constexpr( magic >= ALLEGRO::A_162 )
    {
        parser.Skip( i->size * 280 + 4 );
    }
    else
    {
        parser.Skip( i->size * 240 + 4 );
    }

    return k;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse21( ALLEGRO_PARSER& parser )
{
    auto* i = static_cast<ALLEGRO::T_21_HEADER*>( parser.m_curAddr );
    if( i->r == 1304 )
    {
        ALLEGRO::STACKUP_MATERIAL ps;
        memcpy( &ps, parser.m_curAddr, sizeof( ALLEGRO::STACKUP_MATERIAL ) );
        parser.Skip( sizeof( ALLEGRO::STACKUP_MATERIAL ) );
        // fs.stackup_materials[ps.hdr.k] = ps;
        // log(&f, "- - Stackup material... %02d->%s\n", ps.layer_id,
        // ps.material);
    }
    else if( i->r == 14093 )
    {
        // log(&f, "- - i->r=\x1b[31m14093\x1b[0m...?\n");
        parser.Skip( i->size );
        // } else if (i->r == 2050) {
    }
    else if( i->r == 0x0407 )
    {
        ALLEGRO::META_NETLIST_PATH r;
        parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::META_NETLIST_PATH>() );
        parser.Skip( 1028 );
        // char s[1028];
        // f.read((char*)&r, sizeof_until_tail<meta_netlist_path>());
        // f.read(s, sizeof(s));
        // r.path = std::string(s);
        // fs.netlist_path = r;
    }
    else
    {
        parser.Skip( i->size );
    }
    return 0;
};

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse27( ALLEGRO_PARSER& parser )
{
    parser.m_curAddr = (char*) parser.m_baseAddr + parser.m_header->x27_end_offset - 1;
    return 0;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse2A( ALLEGRO_PARSER& parser )
{
    ALLEGRO::T_2A x2A_inst;
    memcpy( &x2A_inst.hdr, parser.m_curAddr, sizeof( ALLEGRO::x2A_hdr ) );

    // f.read((char*)&x2A_inst.hdr, sizeof(x2A_hdr));
    parser.Skip( sizeof( ALLEGRO::x2A_hdr ) );
    if( magic >= ALLEGRO::A_174 )
    {
        parser.Skip( 4 );
    }

    if( magic <= ALLEGRO::A_164 )
    {
        x2A_inst.references = false;
        for( uint16_t i = 0; i < x2A_inst.hdr.size; i++ )
        {
            char buf[36] = { 0 };
            // f.read((char*)buf, 36);
            memcpy( buf, parser.m_curAddr, 36 );
            parser.Skip( 36 );
            ALLEGRO::x2A_layer_properties suffix = *( (ALLEGRO::x2A_layer_properties*) &buf[32] );
            ALLEGRO::x2A_local_entry entry = ALLEGRO::x2A_local_entry{ std::string( buf ), suffix };
            x2A_inst.local_entries.push_back( entry );
        }
    }
    else
    {
        x2A_inst.references = true;
        for( uint16_t i = 0; i < x2A_inst.hdr.size; i++ )
        {
            ALLEGRO::x2A_reference_entry entry;
            // f.read((char*)&entry, sizeof(x2A_reference_entry));
            memcpy( &entry, parser.m_curAddr, sizeof( ALLEGRO::x2A_reference_entry ) );
            parser.Skip( sizeof( ALLEGRO::x2A_reference_entry ) );
            x2A_inst.reference_entries.push_back( entry );
        }
    }
    // f.read((char*)&x2A_inst.k, 4);
    x2A_inst.k = *static_cast<uint32_t*>( parser.m_curAddr );
    parser.Skip( 4 );
    parser.m_t2A_map[x2A_inst.k] = x2A_inst;
    return 0;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse31( ALLEGRO_PARSER& parser )
{
    auto*    i = static_cast<ALLEGRO::T_31<magic>*>( parser.m_curAddr );
    uint32_t k = ALLEGRO_PARSER<magic>::DefaultParser<ALLEGRO::T_31>( parser );

    if( i->len > 0 )
    {
        uint32_t len = round_to_word( i->len );
        parser.Skip( len );
    }

    return 0;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse35( ALLEGRO_PARSER& parser )
{
    parser.Skip( 124 );
    return 0;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse36( ALLEGRO_PARSER& parser )
{
    auto* inst = static_cast<ALLEGRO::T_36<magic>*>( parser.m_curAddr );
    // memcpy( &inst, parser.m_curAddr, ALLEGRO::sizeof_allegro_obj<ALLEGRO::T_36<magic>>() );
    parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::T_36<magic>>() );
    // f.read((char*)inst, sizeof_until_tail<x36<version>>());

    // log(&f, "- x36\n");
    // log(&f, "- - inst->c    = 0x %04X\n", ntohs(inst->c));
    // log(&f, "- - inst->k    = 0x %08X\n", ntohl(inst->k));
    // log(&f, "- - inst->next = 0x %08X\n", ntohl(inst->next));
    // if constexpr (!std::is_same_v<decltype(inst->un1), std::monostate>) {
    //     log(&f, "- - inst->un1  = 0x %08X\n", ntohl(inst->un1));
    // } else {
    //     log(&f, "- - inst->un1  = n/a\n");
    // }
    // log(&f, "- - inst->size = %d\n", inst->size);
    // log(&f,
    //     "- - inst->count = %d, inst->last_idx = %d, inst->un3 = 0x"
    //     " % 08X\n ",
    //     inst->count, inst->last_idx, ntohl(inst->un3));
    // if constexpr (!std::is_same_v<decltype(inst->un2), std::monostate>) {
    //     log(&f, "- - inst->un2  = 0x %08X\n", ntohl(inst->un2));
    // } else {
    //     log(&f, "- - inst->un2  = n/a\n");
    // }

    switch( inst->c )
    {
    case 0x02:
        for( uint32_t i = 0; i < inst->size; i++ )
        {
            // Discard
            auto inst_x02 = *static_cast<ALLEGRO::x36_x02<magic>*>( parser.m_curAddr );
            parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::x36_x02<magic>>() );
            // f.read((char*)&inst_x02,
            // sizeof_until_tail<x36_x02<version>>()); log(&f, "- x02.str =
            // %s\n", inst_x02.str);
        }
        break;
    case 0x06:
        for( uint32_t i = 0; i < inst->size; i++ )
        {
            // Currently just read the object and immediately throw it
            // away, because we don't know of any use for it.
            auto inst_x06 = *static_cast<ALLEGRO::x36_x06<magic>*>( parser.m_curAddr );
            parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::x36_x06<magic>>() );
            // f.read((char*)&inst_x06,
            // sizeof_until_tail<x36_x06<version>>()); log(&f, "- x06.n =
            // %d\n", inst_x06.n); log(&f, "- x06.r = %d\n", inst_x06.r);
            // log(&f, "- x06.s = %d\n", inst_x06.s);
            // log(&f, "- x06.un1 = %d\n\n", inst_x06.un1);
        }
        break;
    case 0x03:
        for( uint32_t i = 0; i < inst->size; i++ )
        {
            // Just throw this away after reading it.
            auto inst_x03 = *static_cast<ALLEGRO::x36_x03<magic>*>( parser.m_curAddr );
            parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::x36_x03<magic>>() );
            // f.read((char*)&inst_x03,
            // sizeof_until_tail<x36_x03<version>>()); if (version >= A_172)
            // {
            //     log(&f, "- - s = \"%s\"\n", inst_x03.str);
            // } else {
            //     log(&f, "- - s = \"%s\"\n", inst_x03.str_16x);
            // }
        }
        break;
    case 0x05: parser.Skip( inst->size * 28 ); break;
    case 0x08:
        for( uint32_t i = 0; i < inst->size; i++ )
        {
            auto x = *static_cast<ALLEGRO::T_36_08<magic>*>( parser.m_curAddr );
            parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::T_36_08<magic>>() );
            // f.read((char*)&x, sizeof_until_tail<T_36_08<version>>());
            // inst->x08s.push_back( x );
            // log(&f,
            //     "- - 0x%02X %08x %08x"
            //     " xs={h=% 7d w=% 7d % 7d % 7d % 7d %7d}",
            //     i, ntohl(x.a), ntohl(x.b), x.char_height, x.char_width,
            //     x.xs[0], x.xs[1], x.xs[2], x.xs[3]);
            // if constexpr (!std::is_same_v<decltype(x.ys),
            // std::monostate>) {
            //     printf(" ys={% 7d % 7d % 7d}", x.ys[0], x.ys[1],
            //     x.ys[2]);
            // }
            // printf("\n");
        }
        break;
    case 0x0B:
        // No clue what this represents?
        parser.Skip( inst->size * 1016 );
        // for (uint32_t i = 0; i < inst->size; i++) {
        //     log(&f, "- - Next words:");
        //     log_n_words(&f, 6);
        //     skip(&f, 1016 - 4 * 6);
        // }
        break;
    case 0x0C: parser.Skip( inst->size * 232 ); break;
    case 0x0D: parser.Skip( inst->size * 200 ); break;
    case 0x0F:
        for( uint32_t i = 0; i < inst->size; i++ )
        {
            auto x = *static_cast<ALLEGRO::x36_x0F<magic>*>( parser.m_curAddr );
            parser.Skip( sizeof( ALLEGRO::x36_x0F<magic> ) );
            // f.read((char*)&x, sizeof(x36_x0F<version>));
            // inst->x0Fs.push_back( x );
        }
        break;
    case 0x10:
        parser.Skip( inst->size * 108 );
        // for (uint32_t i = 0; i < inst->size; i++) {
        //     log(&f, "- - Next words:");
        //     log_n_words(&f, 6);
        //     skip(&f, 108 - 4 * 6);
        // }
        break;
    default:
        // log( fs.region.get_address(), address, "- Don\'t know how to handle c=%X\n", inst->c );
        parser.Log( "Don\'t know how to handle c=%X", inst->c );
        exit( 1 );
    }

    parser.m_t36_map[inst->k] = inst;
    // parser.m_t36_map.insert( &inst );
    // fs.x36_map[inst->k] = upgrade<version, A_174>( inst );
    return 0;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse3B( ALLEGRO_PARSER& parser )
{
    ALLEGRO::T_3B<magic>* i = (ALLEGRO::T_3B<magic>*) parser.m_curAddr;

    parser.Skip( ALLEGRO::sizeof_allegro_obj<ALLEGRO::T_3B<magic>>() );
    parser.Skip( round_to_word( i->len ) );

    return 0;
}

template <ALLEGRO::MAGIC magic>
uint32_t ALLEGRO_PARSER<magic>::Parse3C( ALLEGRO_PARSER& parser )
{
    auto*    i = static_cast<ALLEGRO::T_3C<magic>*>( parser.m_curAddr );
    uint32_t k = ALLEGRO_PARSER<magic>::DefaultParser<ALLEGRO::T_3C>( parser );

    parser.Skip( i->size * 4 );

    return 0;
}

template <ALLEGRO::MAGIC magic>
uint8_t ALLEGRO_PARSER<magic>::GetCopperLayerCount()
{
    if( m_layerCount > 1 )
    {
        return m_layerCount;
    }

    std::tuple<uint32_t, uint32_t> tup = m_layers[4];
    uint32_t                       ptr = std::get<1>( tup );

    if( m_t2A_map.count( ptr ) > 0 )
    {
        const ALLEGRO::T_2A* x = &m_t2A_map.at( ptr );
        if( x->references )
        {
            m_layerCount = x->reference_entries.size();
        }
        else
        {
            m_layerCount = x->local_entries.size();
        }
        return m_layerCount;
    }
    else
    {
        wxLogMessage( "Unable to determine layer count" );
        return -1;
    }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddAnnotation( BOARD_ITEM_CONTAINER&       aContainer,
                                           const ALLEGRO::T_14<magic>& i14 )
{
    PCB_LAYER_ID layer = User_1;

    if( auto pcb_layer = ALLEGRO::LAYER_MAPPING.find( i14.layer );
        pcb_layer != ALLEGRO::LAYER_MAPPING.end() )
    {
        layer = pcb_layer->second;
    }

    uint32_t k = i14.ptr2;
    while( true )
    {
        if( IsType( k, 0x01 ) )
        {
            auto* i01 = static_cast<ALLEGRO::T_01_ARC<magic>*>( m_ptrs[k] );

            double r = cfp_to_double( i01->r );

            auto [start, end] = GetCoordinatePair( *i01 );
            VECTOR2D center{ Scale( (int32_t) cfp_to_double( i01->x ) ),
                             Scale( -(int32_t) cfp_to_double( i01->y ) ) };
            VECTOR2I mid = CalcArcMid( start, end, center );

            PCB_SHAPE* arc = new PCB_SHAPE( &aContainer, SHAPE_T::ARC );

            arc->SetLayer( layer );
            arc->SetWidth( Scale( i01->width ) );
            arc->SetArcGeometry( start, mid, end );

            aContainer.Add( arc, ADD_MODE::APPEND );

            k = i01->next;
        }
        else if( IsType( k, 0x15 ) )
        {
            auto* i15 = static_cast<ALLEGRO::T_15_SEGMENT<magic>*>( m_ptrs[k] );
            auto [start, end] = GetCoordinatePair( *i15 );
            PCB_SHAPE* segment = new PCB_SHAPE( &aContainer, SHAPE_T::SEGMENT );

            segment->SetLayer( layer );
            segment->SetWidth( Scale( i15->width ) );
            segment->SetStart( start );
            segment->SetEnd( end );

            aContainer.Add( segment, ADD_MODE::APPEND );

            k = i15->next;
        }
        else if( IsType( k, 0x16 ) )
        {
            auto* i16 = static_cast<ALLEGRO::T_16_SEGMENT<magic>*>( m_ptrs[k] );
            auto [start, end] = GetCoordinatePair( *i16 );
            PCB_SHAPE* segment = new PCB_SHAPE( &aContainer, SHAPE_T::SEGMENT );

            segment->SetLayer( layer );
            segment->SetWidth( Scale( i16->width ) );
            segment->SetStart( start );
            segment->SetEnd( end );

            aContainer.Add( segment, ADD_MODE::APPEND );

            k = i16->next;
        }
        else if( IsType( k, 0x17 ) )
        {
            auto* i17 = static_cast<ALLEGRO::T_17_SEGMENT<magic>*>( m_ptrs[k] );
            auto [start, end] = GetCoordinatePair( *i17 );
            PCB_SHAPE* segment = new PCB_SHAPE( &aContainer, SHAPE_T::SEGMENT );

            segment->SetLayer( layer );
            segment->SetWidth( Scale( i17->width ) );
            segment->SetStart( start );
            segment->SetEnd( end );

            aContainer.Add( segment, ADD_MODE::APPEND );

            k = i17->next;
        }
        else
        {
            break;
        }
    }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddFootprint( const ALLEGRO::T_2B_FOOTPRINT<magic>& i2B )
{
    // wxLogMessage( "Adding footprint" );
    wxASSERT_MSG( m_strings.count( i2B.footprint_string_ref ) == 1, "Expected a string" );
    wxString footprint_name = m_strings[i2B.footprint_string_ref];
    LIB_ID   lib_id = LIB_ID( "my_lib", footprint_name );

    uint32_t k = i2B.ptr2;
    while( IsType( k, 0x2D ) )
    {
        auto* i2D = static_cast<ALLEGRO::T_2D_PLACED_FOOTPRINT<magic>*>( m_ptrs[k] );

        std::unique_ptr<FOOTPRINT> fp = std::make_unique<FOOTPRINT>( m_board );
        fp->SetAttributes( FOOTPRINT_ATTR_T::FP_SMD );
        fp->SetFPID( lib_id );

        // Refdes
        std::optional<wxString> refdes = RefdesLookup( *i2D );
        if( refdes )
        {
            fp->SetReference( *refdes );
        }
        else
        {
            fp->SetReference( "A0" );
        }
        fp->Reference().SetVisible( false );

        // Draw pads
        uint32_t k_pad = i2D->first_pad_ptr;
        while( IsType( k_pad, 0x32 ) )
        {
            auto* i32 = static_cast<ALLEGRO::T_32<magic>*>( m_ptrs[k_pad] );
            AddPad( &*fp, *i32 );
            k_pad = i32->next;
        }

        // Position the object
        fp->SetPosition( VECTOR2I( Scale( i2D->coords[0] ), Scale( -i2D->coords[1] ) ) );
        fp->SetOrientationDegrees( ( i2D->layer == 0 ? 1. : -1. ) * i2D->rotation / 1000. );
        fp->SetLayerAndFlip( i2D->layer == 0 ? F_Cu : B_Cu );

        // Add annotations
        uint32_t k_marking = i2D->ptr1;
        while( true )
        {
            if( IsType( k_marking, 0x14 ) )
            {
                auto* i14 = static_cast<ALLEGRO::T_14<magic>*>( m_ptrs[k_marking] );
                AddAnnotation( *fp, *i14 );
                k_marking = i14->next;
            }
            else
            {
                break;
            }
        }

        // Add text
        uint32_t k_text = i2D->ptr3;
        while( true )
        {
            if( IsType( k_text, 0x30 ) )
            {
                auto* i30 = static_cast<ALLEGRO::T_30<magic>*>( m_ptrs[k_text] );
                AddText( *fp, *i30 );
                k_text = i30->next;
            }
            else if( IsType( k_text, 0x03 ) )
            {
                auto* i03 = static_cast<ALLEGRO::T_03<magic>*>( m_ptrs[k_text] );
                // wxLogMessage( "%s T_03.k=%08X", *refdes, ntohl( i03->k ) );
                k_text = i03->next;
            }
            else
            {
                break;
            }
        }

        // Add zones
        // wxLogMessage( "Drawing x2D k=0x%08X zones", ntohl( i2D->k ) );
        uint32_t k_shape = i2D->ptr4[1];
        while( true )
        {
            if( IsType( k_shape, 0x28 ) )
            {
                auto* i28 = static_cast<ALLEGRO::T_28_SHAPE<magic>*>( m_ptrs[k_shape] );
                // wxLogMessage( "Drawing T_28 t=0x%02X.%02X.%02X k=0x%08X", ntohs( i28->type ),
                //               i28->subtype, i28->layer, ntohl( i28->k ) );
                AddZone( *fp, {}, *i28 );
                k_shape = i28->next;
            }
            else if( IsType( k_shape, 0x0E ) )
            {
                auto* i0E = static_cast<ALLEGRO::T_0E<magic>*>( m_ptrs[k_shape] );
                // wxLogMessage( "How to draw T_0E? 0x%08X k=0x%08X", ntohl( i0E->t ),
                //               ntohl( i0E->k ) );
                k_shape = i0E->next;
            }
            else
            {
                break;
            }
        }

        PCB_FIELD* id_field = fp->GetField( FIELD_T::USER );
        id_field->SetName( "ALLEGRO_ID" );
        id_field->SetText( wxString::Format( "0x%08X", ntohl( i2D->k ) ) );
        id_field->SetVisible( false );

        k = i2D->next;

        m_board->Add( fp.release(), ADD_MODE::APPEND );
    }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddPad( FOOTPRINT* fp, const ALLEGRO::T_32<magic>& i32 )
{
    std::unique_ptr<PAD> pad = std::make_unique<PAD>( fp );

    // Find net information
    if( IsType( i32.ptr1, 0x04 ) )
    {
        const auto* i04 = static_cast<ALLEGRO::T_04<magic>*>( m_ptrs[i32.ptr1] );

        NETINFO_ITEM* netinfo = NetInfo( *i04 );
        pad->SetNet( netinfo );
    }

    const auto* i0D = static_cast<ALLEGRO::T_0D<magic>*>( m_ptrs[i32.ptr5] );
    VECTOR2I    center;
    center.x = Scale( i0D->coords[0] );
    center.y = Scale( -i0D->coords[1] );
    pad->SetPosition( center );
    pad->SetOrientationDegrees( i0D->rotation / 1000. );

    std::optional<wxString> pad_number = StringLookup( i0D->str_ptr );
    if( pad_number )
    {
        pad->SetNumber( *pad_number );
    }
    else
    {
        pad->SetNumber( "MISSING" );
    }

    const auto* i1C = static_cast<ALLEGRO::T_1C<magic>*>( m_ptrs[i0D->pad_ptr] );
    // wxLogMessage( "i1C.k = 0x %08X", ntohl( i1C->k ) );

    switch( i1C->pad_info.pad_type )
    {
    case ALLEGRO::SmtPin:
    case ALLEGRO::SmtPin2:
        pad->SetAttribute( PAD_ATTRIB::SMD );
        pad->SetLayerSet( PAD::SMDMask() );
        break;
    case ALLEGRO::ThroughVia:
    case ALLEGRO::Slot:
        pad->SetAttribute( PAD_ATTRIB::PTH );
        pad->SetLayerSet( PAD::PTHMask() );
        break;
    case ALLEGRO::NonPlatedHole:
        pad->SetAttribute( PAD_ATTRIB::NPTH );
        pad->SetLayerSet( PAD::UnplatedHoleMask() );
        break;
    default: wxLogMessage( "Unknown pad type %d", i1C->pad_info.pad_type );
    }

    /*
    const uint32_t component_count = GetPadComponentCount( *i1C );
    wxLogMessage( "next pad, for 0x1C k=0x %08X", ntohl( i1C->k ) );
    for( uint8_t i = 0; i < component_count; i++ )
    {
        ALLEGRO::t13<magic>* c = GetPadComponent( *i1C, i );
        wxLogMessage( "%02d: t=0x%08X, ptr=0x%08X", i, c->t, ntohl( c->str_ptr ) );
    }
    */

    uint8_t offset = 23;
    if constexpr( magic < ALLEGRO::A_172 )
    {
        offset = 12;
    }
    ALLEGRO::t13<magic>* comp = GetPadComponent( *i1C, offset );
    if( comp->t != 0 )
    {
        SetPadShape( *pad, *comp );
        fp->Add( pad.release(), ADD_MODE::APPEND );
    }

    uint8_t mask_offset = 14;
    if constexpr( magic < ALLEGRO::A_172 )
    {
        mask_offset = 0;
    }
    comp = GetPadComponent( *i1C, mask_offset );
    if( comp->t != 0 )
    {
        std::unique_ptr<PAD> mask_pad = std::make_unique<PAD>( fp );
        mask_pad->SetAttribute( PAD_ATTRIB::SMD );
        mask_pad->SetLayer( F_Mask );
        mask_pad->SetLayerSet( LSET().set( F_Mask ) );
        mask_pad->SetPosition( center );
        mask_pad->SetOrientationDegrees( i0D->rotation / 1000. );
        SetPadShape( *mask_pad, *comp );
        fp->Add( mask_pad.release(), ADD_MODE::APPEND );
    }

    uint8_t paste_offset = 16;
    if constexpr( magic < ALLEGRO::A_172 )
    {
        paste_offset = 5;
    }
    comp = GetPadComponent( *i1C, paste_offset );
    if( comp->t != 0 )
    {
        std::unique_ptr<PAD> paste_pad = std::make_unique<PAD>( fp );
        paste_pad->SetAttribute( PAD_ATTRIB::SMD );
        paste_pad->SetLayer( F_Paste );
        paste_pad->SetLayerSet( LSET().set( F_Paste ) );
        paste_pad->SetPosition( center );
        paste_pad->SetOrientationDegrees( i0D->rotation / 1000. );
        SetPadShape( *paste_pad, *comp );
        fp->Add( paste_pad.release(), ADD_MODE::APPEND );
    }

    // Add label
    // FIXME: This is positioned incorrectly.
    // if( IsType( i32.ptr10, 0x30 ) )
    // {
    //     ALLEGRO::T_30<magic>* i30 = static_cast<ALLEGRO::T_30<magic>*>( m_ptrs[i32.ptr10] );
    //     AddText( *fp, *i30 );
    // }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddText( BOARD_ITEM_CONTAINER&       aContainer,
                                     const ALLEGRO::T_30<magic>& i30 )
{
    auto* i31 = static_cast<ALLEGRO::T_31<magic>*>( m_ptrs[i30.str_graphic_ptr] );

    const char* s = (char*) ( &i31->TAIL );

    if( strlen( s ) == 0 )
    {
        // wxLogMessage( "empty string" );
    }
    else
    {
        std::unique_ptr<PCB_TEXT> t = std::make_unique<PCB_TEXT>( &aContainer );

        // FIXME: This probably needs to be flipped relative to the placement
        PCB_LAYER_ID layer = User_2;
        switch( i31->layer )
        {
        case ALLEGRO::STR_LAYER::TOP_TEXT:
        case ALLEGRO::STR_LAYER::TOP_PIN:
        case ALLEGRO::STR_LAYER::TOP_REFDES: layer = F_SilkS; break;
        case ALLEGRO::STR_LAYER::BOT_TEXT:
        case ALLEGRO::STR_LAYER::BOT_PIN:
        case ALLEGRO::STR_LAYER::BOT_REFDES: layer = B_SilkS; break;
        case ALLEGRO::STR_LAYER::TOP_PIN_LABEL: layer = User_5; break;
        }

        t->SetLayer( layer );
        t->SetVertJustify( GR_TEXT_V_ALIGN_BOTTOM );
        t->SetPosition( VECTOR2I( Scale( i30.coords[0] ), Scale( -i30.coords[1] ) ) );
        t->Rotate( t->GetPosition(), EDA_ANGLE( i30.rotation / 1000., DEGREES_T ) );
        t->SetText( wxString( s ) );

        ALLEGRO::TEXT_PROPERTIES props;
        if constexpr( !std::is_same_v<decltype( i30.font ), std::monostate> )
        {
            props = i30.font;
        }
        else
        {
            props = i30.font_16x;
        }

        switch( props.align )
        {
        case ALLEGRO::TEXT_ALIGNMENT::TextAlignLeft:
            t->SetHorizJustify( GR_TEXT_H_ALIGN_LEFT );
            break;
        case ALLEGRO::TEXT_ALIGNMENT::TextAlignCenter:
            t->SetHorizJustify( GR_TEXT_H_ALIGN_CENTER );
            break;
        case ALLEGRO::TEXT_ALIGNMENT::TextAlignRight:
            t->SetHorizJustify( GR_TEXT_H_ALIGN_RIGHT );
            break;
        }

        if( props.reversed == ALLEGRO::TEXT_REVERSAL::TextReversed )
        {
            t->SetMirrored( true );
        }

        const ALLEGRO::T_36_08<magic>* font = FindFont( props.key );
        if( font != nullptr )
        {
            t->SetTextSize( VECTOR2I( Scale( font->char_width ), Scale( font->char_height ) ) );
        }
        else
        {
            wxLogMessage( "Font == null" );
        }

        aContainer.Add( t.release(), ADD_MODE::APPEND );
    }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddTrack( const ALLEGRO::T_1B<magic>&       i1B,
                                      const ALLEGRO::T_05_TRACK<magic>& i05 )
{
    uint32_t k = i05.first_segment_ptr;

    NETINFO_ITEM* netinfo = NetInfo( i1B.net_name );

    while( true )
    {
        if( IsType( k, 0x01 ) )
        {
            auto* i01 = static_cast<ALLEGRO::T_01_ARC<magic>*>( m_ptrs[k] );

            double r = cfp_to_double( i01->r );

            VECTOR2I start, end, center, mid;
            start.x = Scale( i01->coords[0] );
            start.y = Scale( -i01->coords[1] );
            end.x = Scale( i01->coords[2] );
            end.y = Scale( -i01->coords[3] );
            center.x = Scale( (int32_t) cfp_to_double( i01->x ) );
            center.y = Scale( -(int32_t) cfp_to_double( i01->y ) );

            SHAPE_ARC shape_arc;
            shape_arc.ConstructFromStartEndCenter( start, end, center, i01->subtype == 0x00, 0 );

            std::unique_ptr<PCB_ARC> arc = std::make_unique<PCB_ARC>( m_board, &shape_arc );

            arc->SetLayer( EtchLayerToKi( i05.layer ) );
            arc->SetWidth( Scale( i01->width ) );
            arc->SetNet( netinfo );

            m_board->Add( arc.release(), ADD_MODE::APPEND );

            k = i01->next;
        }
        else if( IsType( k, 0x15 ) )
        {
            auto* i15 = static_cast<ALLEGRO::T_15_SEGMENT<magic>*>( m_ptrs[k] );

            VECTOR2D start, end;
            start.x = Scale( i15->coords[0] );
            start.y = Scale( -i15->coords[1] );
            end.x = Scale( i15->coords[2] );
            end.y = Scale( -i15->coords[3] );

            std::unique_ptr<PCB_TRACK> track = std::make_unique<PCB_TRACK>( m_board );

            track->SetLayer( EtchLayerToKi( i05.layer ) );
            track->SetWidth( Scale( i15->width ) );
            track->SetStart( start );
            track->SetEnd( end );
            track->SetNet( netinfo );

            m_board->Add( track.release(), ADD_MODE::APPEND );

            k = i15->next;
        }
        else if( IsType( k, 0x16 ) )
        {
            auto* i16 = static_cast<ALLEGRO::T_16_SEGMENT<magic>*>( m_ptrs[k] );

            VECTOR2D start, end;
            start.x = Scale( i16->coords[0] );
            start.y = Scale( -i16->coords[1] );
            end.x = Scale( i16->coords[2] );
            end.y = Scale( -i16->coords[3] );

            std::unique_ptr<PCB_TRACK> track = std::make_unique<PCB_TRACK>( m_board );

            track->SetLayer( EtchLayerToKi( i05.layer ) );
            track->SetWidth( Scale( i16->width ) );
            track->SetStart( start );
            track->SetEnd( end );
            track->SetNet( netinfo );

            m_board->Add( track.release(), ADD_MODE::APPEND );

            k = i16->next;
        }
        else if( IsType( k, 0x17 ) )
        {
            auto* i17 = static_cast<ALLEGRO::T_17_SEGMENT<magic>*>( m_ptrs[k] );

            VECTOR2D start, end;
            start.x = Scale( i17->coords[0] );
            start.y = Scale( -i17->coords[1] );
            end.x = Scale( i17->coords[2] );
            end.y = Scale( -i17->coords[3] );

            std::unique_ptr<PCB_TRACK> track = std::make_unique<PCB_TRACK>( m_board );

            track->SetLayer( EtchLayerToKi( i05.layer ) );
            track->SetWidth( Scale( i17->width ) );
            track->SetStart( start );
            track->SetEnd( end );
            track->SetNet( netinfo );

            m_board->Add( track.release(), ADD_MODE::APPEND );

            k = i17->next;
        }
        else
        {
            break;
        }
    }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddVia( const ALLEGRO::T_33<magic>& i33 )
{
    std::unique_ptr<PCB_VIA> via = std::make_unique<PCB_VIA>( m_board );
    via->SetPosition( VECTOR2D( Scale( i33.coords[0] ), Scale( -i33.coords[1] ) ) );

    // Find net information
    if( IsType( i33.ptr1, 0x04 ) )
    {
        const auto* i04 = static_cast<ALLEGRO::T_04<magic>*>( m_ptrs[i33.ptr1] );

        NETINFO_ITEM* netinfo = NetInfo( *i04 );
        via->SetNet( netinfo );
    }

    m_board->Add( via.release(), ADD_MODE::APPEND );
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddZone( BOARD_ITEM_CONTAINER&                      aContainer,
                                     const std::optional<ALLEGRO::T_1B<magic>>& i1B,
                                     const ALLEGRO::T_28_SHAPE<magic>&          i28 )
{
    uint32_t k = i28.first_segment_ptr;

    std::unique_ptr<SHAPE_POLY_SET> outline = std::make_unique<SHAPE_POLY_SET>();
    outline->AddOutline( ShapeStartingAt( &k ) );

    uint32_t hole_k = i28.ptr4;
    while( IsType( hole_k, 0x34 ) )
    {
        auto* i34 = static_cast<ALLEGRO::T_34<magic>*>( m_ptrs[hole_k] );
        // wxLogMessage( "Handling x34 0x%08X", ntohl( i34->k ) );
        uint32_t k = i34->ptr2;

        SHAPE_LINE_CHAIN hole_chain = ShapeStartingAt( &k );
        if( hole_chain.PointCount() >= 3 )
        {
            hole_chain.SetClosed( true );
            // zone->AddPolygon( hole_chain );
            outline->AddHole( hole_chain );
        }
        else
        {
            // wxLogMessage( "Skipping, why?" );
        }
        hole_k = i34->next;
    }

    if( i28.layer.family == ALLEGRO::LAYER_FAMILY::COPPER )
    {
        std::unique_ptr<ZONE> zone = std::make_unique<ZONE>( &aContainer );
        zone->SetZoneName( wxString::Format( "x28: 0x%08X", ntohl( i28.k ) ) );
        zone->SetLocalClearance( 0 );
        zone->SetMinThickness( 0 );
        zone->SetOutline( outline.release() );

        PCB_LAYER_ID layer = EtchLayerToKi( i28.layer );

        zone->SetLayer( layer );

        if( i1B )
        {
            NETINFO_ITEM* netinfo = NetInfo( i1B->net_name );
            zone->SetNet( netinfo );
        }

        // zone->SetFilledPolysList( layer, outline );

        aContainer.Add( zone.release(), ADD_MODE::APPEND );
    }
    else if( i28.layer.family == ALLEGRO::LAYER_FAMILY::BOARD_GEOMETRY
             && i28.layer.ordinal == 0xFD )
    {
        std::unique_ptr<PCB_SHAPE> shape =
                std::make_unique<PCB_SHAPE>( &aContainer, SHAPE_T::POLY );
        shape->SetPolyShape( *outline.release() );
        shape->SetFilled( false );
        shape->SetLayer( Edge_Cuts );
        aContainer.Add( shape.release(), ADD_MODE::APPEND );
    }
    else if( i28.layer.family == ALLEGRO::LAYER_FAMILY::SILK )
    {
        std::unique_ptr<PCB_SHAPE> shape =
                std::make_unique<PCB_SHAPE>( &aContainer, SHAPE_T::POLY );
        shape->SetPolyShape( *outline.release() );
        shape->SetFilled( true );

        if( i28.layer.ordinal == 0xFD )
        {
            shape->SetLayer( F_SilkS );
        }
        else if( i28.layer.ordinal == 0xEC )
        {
            shape->SetWidth( 0 );
            shape->SetLayer( B_Mask );
        }
        else if( i28.layer.ordinal == 0xED )
        {
            shape->SetWidth( 0 );
            shape->SetLayer( F_Mask );
        }
        else if( i28.layer.ordinal == 0xF6 )
        {
            shape->SetFilled( false );
            shape->SetLayer( F_SilkS );
        }
        else if( i28.layer.ordinal == 0xFA )
        {
            shape->SetLayer( B_CrtYd );
        }
        else if( i28.layer.ordinal == 0xFB )
        {
            shape->SetLayer( F_CrtYd );
        }
        else if( i28.layer.ordinal == 0xF3 )
        {
            shape->SetLayer( B_Paste );
        }
        else if( i28.layer.ordinal == 0xF4 )
        {
            shape->SetLayer( F_Paste );
        }
        else if( i28.layer.ordinal == 0xF7 )
        {
            shape->SetLayer( User_6 );
        }
        else if( i28.layer.ordinal == 0xEE )
        {
            // What are these?
            shape->SetFilled( false );
            shape->SetLayer( User_7 );
        }
        else if( i28.layer.ordinal == 0xEF )
        {
            // What are these?
            shape->SetFilled( false );
            shape->SetLayer( User_8 );
        }
        else if( i28.layer.ordinal == 0x02 )
        {
            shape->SetFilled( false );
            shape->SetLayer( Eco1_User );
        }
        else if( i28.layer.ordinal == 0x00 )
        {
            shape->SetFilled( false );
            shape->SetLayer( Eco2_User );
        }
        else
        {
            // wxLogMessage( "Adding zone from 09.%02X", i28.layer );
            shape->SetLayer( User_3 );
        }

        aContainer.Add( shape.release(), ADD_MODE::APPEND );
    }
    /*
    else if( i28.layer.family == ALLEGRO::ZONE_TYPE_TBD )

    {
        // PCB_LAYER_ID layer = (PCB_LAYER_ID) ( ( (int) F_Cu ) + i28.layer );
        zone->SetLayer( F_Cu );

        // zone->SetIsRuleArea( true );
        // zone->SetDoNotAllowCopperPour( true );

        aContainer.Add( zone.release(), ADD_MODE::APPEND );
    }
    */
    else
    {
        // zone->SetLayer( User_3 );
        // aContainer.Add( zone.release(), ADD_MODE::APPEND );
    }
}

// This is typically used to frame things, like tables or pages.
template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::AddRectangle( BOARD_ITEM_CONTAINER&       aContainer,
                                          const ALLEGRO::T_24<magic>& i24 )
{
    std::unique_ptr<SHAPE_POLY_SET> outline = std::make_unique<SHAPE_POLY_SET>();

    SHAPE_LINE_CHAIN chain;
    auto [a, b] = GetCoordinatePair( i24 );
    chain.Append( a.x, a.y );
    chain.Append( a.x, b.y );
    chain.Append( b.x, b.y );
    chain.Append( b.x, a.y );
    chain.SetClosed( true );

    outline->AddOutline( chain );

    std::unique_ptr<PCB_SHAPE> shape = std::make_unique<PCB_SHAPE>( &aContainer, SHAPE_T::POLY );
    shape->SetPolyShape( *outline.release() );
    shape->SetFilled( false );
    shape->SetLayer( User_3 );
    aContainer.Add( shape.release(), ADD_MODE::APPEND );
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::BuildBoard()
{
    m_board->SetCopperLayerCount( GetCopperLayerCount() );

    auto* i = static_cast<ALLEGRO::T_1B<magic>*>( m_ptrs[m_header->ll_x1B.head] );
    if( i != nullptr )
    {
        do
        {
            if( i->ptr1 != 0 )
            {
                auto* i04 = static_cast<ALLEGRO::T_04<magic>*>( m_ptrs[i->ptr1] );
                do
                {
                    uint32_t k = i04->ptr2;
                    while( true )
                    {
                        if( IsType( k, 0x33 ) )
                        {
                            auto* i33 = static_cast<ALLEGRO::T_33<magic>*>( m_ptrs[k] );
                            // auto& i = fs->get_x33( k );
                            // printf("- - Found x33 w/ key = 0x %08X\n", ntohl(k));
                            AddVia( *i33 );
                            k = i33->un1;
                        }
                        else if( IsType( k, 0x32 ) )
                        {
                            auto* i = static_cast<ALLEGRO::T_32<magic>*>( m_ptrs[k] );
                            // printf("- - Found x32 w/ key = 0x %08X\n", ntohl(k));
                            k = i->un1;
                        }
                        else if( IsType( k, 0x2E ) )
                        {
                            auto* i = static_cast<ALLEGRO::T_2E<magic>*>( m_ptrs[k] );
                            // printf("- - Found x2E w/ key = 0x %08X\n", ntohl(k));
                            k = i->un[0];
                        }
                        else if( IsType( k, 0x28 ) )
                        {
                            auto* i28 = static_cast<ALLEGRO::T_28_SHAPE<magic>*>( m_ptrs[k] );
                            AddZone( *m_board, *i, *i28 );

                            k = i28->next;
                        }
                        else if( IsType( k, 0x0E ) )
                        {
                            auto* i = static_cast<ALLEGRO::T_0E<magic>*>( m_ptrs[k] );
                            k = i->un[0];
                        }
                        else if( IsType( k, 0x05 ) )
                        {
                            auto* i05 = static_cast<ALLEGRO::T_05_TRACK<magic>*>( m_ptrs[k] );
                            AddTrack( *i, *i05 );

                            k = i05->ptr0;
                        }
                        else if( IsType( k, 0x04 ) | IsType( k, 0x1B ) )
                        {
                            break;
                        }
                        else
                        {
                            // wxLogMessage( "Unexpected key = 0x %08X :(", ntohl( k ) );
                            break;
                        }
                    }

                    if( i04->next == i->k )
                    {
                        break;
                    }
                    else
                    {
                        i04 = static_cast<ALLEGRO::T_04<magic>*>( m_ptrs[i04->next] );
                    }
                } while( true );
            }

            i = static_cast<ALLEGRO::T_1B<magic>*>( m_ptrs[i->next] );
        } while( i->next != m_header->ll_x1B.tail );
    }

    uint32_t k = m_header->ll_x0E_x28.head;
    if( k != 0 )
    {
        while( k != m_header->ll_x0E_x28.tail )
        {
            if( IsType( k, 0x0E ) )
            {
                // Do not know what this represents
                auto i0E = static_cast<ALLEGRO::T_0E<magic>*>( m_ptrs[k] );
                k = i0E->next;
            }
            else if( IsType( k, 0x28 ) )
            {
                auto i28 = static_cast<ALLEGRO::T_28_SHAPE<magic>*>( m_ptrs[k] );
                AddZone( *m_board, {}, *i28 );
                k = i28->next;
            }
            else
            {
                // wxLogMessage( "Unexpected type with key = 0x%08X!", ntohl( k ) );
                break;
            }
        }
    }

    k = m_header->ll_x14.head;
    if( k != 0 )
    {
        while( k != m_header->ll_x14.tail )
        {
            auto i14 = static_cast<ALLEGRO::T_14<magic>*>( m_ptrs[k] );
            AddAnnotation( *m_board, *i14 );
            k = i14->next;
        }
    }

    k = m_header->ll_x2B.head;
    if( k != 0 )
    {
        while( k != m_header->ll_x2B.tail )
        {
            auto i2B = static_cast<ALLEGRO::T_2B_FOOTPRINT<magic>*>( m_ptrs[k] );
            AddFootprint( *i2B );
            k = i2B->next;
        }
    }

    k = m_header->ll_x03_x30.head;
    if( k != 0 )
    {
        while( k != m_header->ll_x03_x30.tail )
        {
            if( IsType( k, 0x30 ) )
            {
                auto* i30 = static_cast<ALLEGRO::T_30<magic>*>( m_ptrs[k] );
                AddText( *m_board, *i30 );
                k = i30->next;
            }
            else if( IsType( k, 0x03 ) )
            {
                auto* i03 = static_cast<ALLEGRO::T_03<magic>*>( m_ptrs[k] );
                k = i03->next;
            }
            else
            {
                // wxLogMessage( "Unexpected type in ll_x03_x30" );
                break;
            }
        }
    }

    k = m_header->ll_x24_x28.head;
    if( k != 0 )
    {
        while( k != m_header->ll_x24_x28.tail )
        {
            if( IsType( k, 0x24 ) )
            {
                auto* i = static_cast<ALLEGRO::T_24<magic>*>( m_ptrs[k] );
                AddRectangle( *m_board, *i );
                k = i->next;
            }
            else if( IsType( k, 0x28 ) )
            {
                auto* i = static_cast<ALLEGRO::T_28_SHAPE<magic>*>( m_ptrs[k] );
                AddZone( *m_board, {}, *i );
                k = i->next;
            }
            else
            {
                THROW_IO_ERROR( wxString::Format(
                        "Expected t=0x24 or t=0x28 but k=0x%08X unexpected", ntohl( k ) ) );
            }
        }
    }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::Skip( std::size_t n )
{
    m_curAddr = (void*) ( ( (char*) m_curAddr ) + n );
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::Log( const char* fmt... )
{
    va_list args;
    va_start( args, fmt );

    wxLogMessage( "@ 0x%08lX:", (char*) m_curAddr - (char*) m_baseAddr );
    wxVLogMessage( fmt, args );

    va_end( args );
}

template <ALLEGRO::MAGIC magic>
bool ALLEGRO_PARSER<magic>::IsType( uint32_t k, uint8_t t )
{
    return ( m_ptrs.count( k ) > 0 ) && ( *(uint8_t*) m_ptrs[k] == t );
}

template <ALLEGRO::MAGIC magic>
std::optional<wxString> ALLEGRO_PARSER<magic>::StringLookup( uint32_t k )
{
    if( m_strings.count( k ) > 0 )
    {
        return wxString( m_strings[k] );
    }
    return {};
}

template <ALLEGRO::MAGIC magic>
SHAPE_LINE_CHAIN ALLEGRO_PARSER<magic>::ShapeStartingAt( uint32_t* k )
{
    SHAPE_LINE_CHAIN chain;

    bool first = true;
    while( k != nullptr )
    {
        if( IsType( *k, 0x01 ) )
        {
            auto* i01 = static_cast<ALLEGRO::T_01_ARC<magic>*>( m_ptrs[*k] );

            auto [start, end] = GetCoordinatePair( *i01 );
            VECTOR2D center{ Scale( (int32_t) cfp_to_double( i01->x ) ),
                             Scale( -(int32_t) cfp_to_double( i01->y ) ) };

            SHAPE_ARC arc;
            arc.ConstructFromStartEndCenter( start, end, center, i01->subtype == 0x00, 0 );

            if( first )
            {
                chain.Append( start );
            }
            chain.Append( arc );

            *k = i01->next;
        }
        else if( IsType( *k, 0x15 ) )
        {
            auto* segment_inst = static_cast<ALLEGRO::T_15_SEGMENT<magic>*>( m_ptrs[*k] );
            AddSegmentToChain( chain, segment_inst, first );
            *k = segment_inst->next;
        }
        else if( IsType( *k, 0x16 ) )
        {
            auto* segment_inst = static_cast<ALLEGRO::T_16_SEGMENT<magic>*>( m_ptrs[*k] );
            AddSegmentToChain( chain, segment_inst, first );
            *k = segment_inst->next;
        }
        else if( IsType( *k, 0x17 ) )
        {
            auto* segment_inst = static_cast<ALLEGRO::T_17_SEGMENT<magic>*>( m_ptrs[*k] );
            AddSegmentToChain( chain, segment_inst, first );
            *k = segment_inst->next;
        }
        else
        {
            break;
        }

        first = false;
    }

    chain.SetClosed( true );
    return chain;
}

template <ALLEGRO::MAGIC magic>
template <class SEGMENT_TYPE>
void ALLEGRO_PARSER<magic>::AddSegmentToChain( SHAPE_LINE_CHAIN& chain, SEGMENT_TYPE* segment,
                                               bool first_segment )
{
    auto [start, end] = GetCoordinatePair( *segment );
    if( first_segment )
    {
        chain.Append( start );
    }
    chain.Append( end );
}

template <ALLEGRO::MAGIC magic>
PCB_LAYER_ID ALLEGRO_PARSER<magic>::EtchLayerToKi( ALLEGRO::LAYER_INFO a_layer )
{
    if( a_layer.ordinal == 0 )
    {
        return F_Cu;
    }
    else if( a_layer.ordinal == GetCopperLayerCount() - 1 )
    {
        return B_Cu;
    }
    else
    {
        return static_cast<PCB_LAYER_ID>( B_Cu + ( a_layer.ordinal * 2 ) );
    }
}

template <ALLEGRO::MAGIC magic>
double ALLEGRO_PARSER<magic>::Scale( double a_allegroValue )
{
    return a_allegroValue * m_scaleFactor;
}

template <ALLEGRO::MAGIC magic>
template <class T>
std::pair<VECTOR2D, VECTOR2D> ALLEGRO_PARSER<magic>::GetCoordinatePair( T& inst )
{
    return { { Scale( inst.coords[0] ), Scale( -inst.coords[1] ) },
             { Scale( inst.coords[2] ), Scale( -inst.coords[3] ) } };
}

template <ALLEGRO::MAGIC magic>
const ALLEGRO::T_36_08<magic>* ALLEGRO_PARSER<magic>::FindFont( uint8_t k )
{
    // Unclear why there's an offset, but this seems to make things
    // work nicely?
    constexpr int8_t offset = -1;

    for( const auto& [_k, x36_inst] : this->m_t36_map )
    {
        // FIXME: Throw an error if `k` is larger than the size of the list
        if( x36_inst->c == 0x08 )
        {
            return reinterpret_cast<ALLEGRO::T_36_08<magic>*>(
                    ( (char*) x36_inst ) + ALLEGRO::sizeof_allegro_obj<ALLEGRO::T_36<magic>>()
                    + ALLEGRO::sizeof_allegro_obj<ALLEGRO::T_36_08<magic>>() * ( k + offset ) );
        }
    }

    wxLogMessage( "Failed to find font" );
    return nullptr;
}

template <ALLEGRO::MAGIC magic>
constexpr uint32_t ALLEGRO_PARSER<magic>::GetPadComponentCount( const ALLEGRO::T_1C<magic>& i1C )
{
    if( magic < ALLEGRO::A_172 )
    {
        return 10 + 3 * i1C.layer_count;
    }
    else
    {
        return 21 + 4 * i1C.layer_count;
    }
}

template <ALLEGRO::MAGIC magic>
void ALLEGRO_PARSER<magic>::SetPadShape( PAD& pad, const ALLEGRO::t13<magic>& it13 )
{
    switch( it13.t )
    {
    case 0x02: pad.SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::CIRCLE ); break;
    case 0x05:
    case 0x06: pad.SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::RECTANGLE ); break;
    case 0x0B:
    case 0x1B:
    case 0x0C: pad.SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::ROUNDRECT ); break;
    case 0x16: pad.SetShape( PADSTACK::ALL_LAYERS, PAD_SHAPE::CUSTOM ); break;
    default: wxLogMessage( "Unrecognized type: t=%02X", it13.t );
    }

    if( it13.t == 0x16 )
    {
        if( IsType( it13.str_ptr, 0x28 ) )
        {
            auto*    i28 = static_cast<ALLEGRO::T_28_SHAPE<magic>*>( m_ptrs[it13.str_ptr] );
            uint32_t k = i28->first_segment_ptr;
            pad.DeletePrimitivesList();
            pad.AddPrimitivePoly( PADSTACK::ALL_LAYERS, ShapeStartingAt( &k ), 0, true );

            // FIXME: Set the fake pad to a very small size
            pad.SetSize( PADSTACK::ALL_LAYERS,
                         VECTOR2I( pcbIUScale.MilsToIU( 1 ), pcbIUScale.MilsToIU( 1 ) ) );
        }
        else
        {
            wxLogMessage( "Weird case?, str_ptr = 0x %08X, count = %d", ntohl( it13.str_ptr ),
                          m_ptrs.count( it13.str_ptr ) );
        }
    }
    else
    {
        pad.SetSize( PADSTACK::ALL_LAYERS, VECTOR2I( Scale( it13.w ), Scale( it13.h ) ) );
    }
    pad.Move( VECTOR2I( Scale( it13.x3 ), -Scale( it13.x4 ) ) );
}

template <ALLEGRO::MAGIC magic>
constexpr ALLEGRO::t13<magic>*
ALLEGRO_PARSER<magic>::GetPadComponent( const ALLEGRO::T_1C<magic>& i1C, uint32_t idx )
{
    return (ALLEGRO::t13<magic>*) ( (char*) &i1C
                                    + ALLEGRO::sizeof_allegro_obj<ALLEGRO::T_1C<magic>>()
                                    + ALLEGRO::sizeof_allegro_obj<ALLEGRO::t13<magic>>() * idx );
}

template <ALLEGRO::MAGIC magic>
std::optional<wxString>
ALLEGRO_PARSER<magic>::RefdesLookup( const ALLEGRO::T_2D_PLACED_FOOTPRINT<magic>& i2D )
{
    uint32_t inst_ref = 0;
    if constexpr( std::is_same_v<decltype( i2D.inst_ref ), std::monostate> )
    {
        inst_ref = i2D.inst_ref_16x;
    }
    else
    {
        inst_ref = i2D.inst_ref;
    }

    if( inst_ref == 0 || !IsType( inst_ref, 0x07 ) )
        return {};

    auto* i07 = static_cast<ALLEGRO::T_07<magic>*>( m_ptrs[inst_ref] );
    wxASSERT_MSG( m_strings.count( i07->refdes_string_ref ) == 1, "Expected a string" );
    return wxString( m_strings[i07->refdes_string_ref] );
}

template <ALLEGRO::MAGIC magic>
NETINFO_ITEM* ALLEGRO_PARSER<magic>::NetInfo( uint32_t k )
{
    NETINFO_ITEM*           netinfo = nullptr;
    std::optional<wxString> netname = StringLookup( k );

    if( netname )
    {
        if( NETINFO_ITEM* item = m_board->FindNet( *netname ) )
        {
            netinfo = item;
        }
        else
        {
            item = new NETINFO_ITEM( m_board, *netname, m_board->GetNetCount() + 1 );
            m_board->Add( item, ADD_MODE::APPEND );
            netinfo = item;
        }
    }
    return netinfo;
}

template <ALLEGRO::MAGIC magic>
NETINFO_ITEM* ALLEGRO_PARSER<magic>::NetInfo( const ALLEGRO::T_04<magic>& i04 )
{
    auto* i1B = static_cast<ALLEGRO::T_1B<magic>*>( m_ptrs[i04.ptr1] );
    return NetInfo( i1B->net_name );
}

template class ALLEGRO_PARSER<ALLEGRO::A_160>;
template class ALLEGRO_PARSER<ALLEGRO::A_162>;
template class ALLEGRO_PARSER<ALLEGRO::A_164>;
template class ALLEGRO_PARSER<ALLEGRO::A_165>;
template class ALLEGRO_PARSER<ALLEGRO::A_166>;
template class ALLEGRO_PARSER<ALLEGRO::A_172>;
template class ALLEGRO_PARSER<ALLEGRO::A_174>;
template class ALLEGRO_PARSER<ALLEGRO::A_175>;
