#ifndef ALLEGRO_ELEM_PARSERS_H_
#define ALLEGRO_ELEM_PARSERS_H_

#include "allegro_structs.h"

/*
template <ALLEGRO::MAGIC magic>
class ALLEGRO_PARSER;

template <template <ALLEGRO::MAGIC> typename T, ALLEGRO::MAGIC magic>
uint32_t default_parser( ALLEGRO_PARSER<magic>& parser, void*& address );

template <ALLEGRO::MAGIC magic>
struct PARSER_T
{
    uint32_t ( *parse )( ALLEGRO_PARSER<magic>& parser, void*& address );
};

template <ALLEGRO::MAGIC magic>
const PARSER_T<magic> PARSER_TABLE[] = {
    // 0x00
    {},
    // 0x01
    { &default_parser<ALLEGRO::HEADER, magic> },
};
*/

#endif // ALLEGRO_ELEM_PARSERS_H_
