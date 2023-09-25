#ifndef ALLEGRO_PCB_H_
#define ALLEGRO_PCB_H_

#include <pcb_io/pcb_io.h>

#include "allegro_file.h"
#include "allegro_structs.h"

class ALLEGRO_PCB
{
public:
    ALLEGRO_PCB( BOARD* aBoard );
    ~ALLEGRO_PCB() = default;

    void Parse( const ALLEGRO_FILE& aAllegroBrdFile );

private:
    BOARD* m_board;
};

#endif // ALLEGRO_PCB_H_
