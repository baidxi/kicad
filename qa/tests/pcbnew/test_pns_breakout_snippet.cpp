BOOST_AUTO_TEST_CASE( PNS_DiffPairBreakout_Spread )
{
    // Gap = 200um (width 100 + gap 100)
    int gap = 200000;
    PNS::DP_GATEWAYS gateways( gap );
    
    // Spread Case: pads at 0.1mm distance
    VECTOR2I p0_p( 0, 0 );
    VECTOR2I p0_n( 0, 100000 );
    
    SHAPE_RECT rect( VECTOR2I( -25000, -25000 ), 50000, 50000 );
    
    PNS::SOLID solidP;
    solidP.SetShape( &rect );
    solidP.SetPos( p0_p );
    
    PNS::SOLID solidN;
    solidN.SetShape( &rect );
    solidN.SetPos( p0_n );

    PNS::DP_PRIMITIVE_PAIR pair( &solidP, &solidN );
    pair.SetAnchors( p0_p, p0_n );
    
    gateways.BuildFromPrimitivePair( pair, false );
    
    // Now verify we got gateways.
    BOOST_CHECK( gateways.Gateways().size() > 0 );
    
    bool foundSpread = false;
    for( const auto& gw : gateways.Gateways() )
    {
         int dist = ( gw.AnchorP() - gw.AnchorN() ).EuclideanNorm();
         // Check if distance is close to gap (200000)
         // Allow some tolerance as fp calculation involves sqrt/rounding
         if( std::abs( dist - gap ) < 10000 ) // 5% tolerance
             foundSpread = true;
    }
    BOOST_CHECK_MESSAGE( foundSpread, "Failed to find spreading gateway" );
}
