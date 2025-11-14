#include "idx_reader.h"

IDX_HOLDER::IDX_HOLDER( wxXmlDocument& aDoc, bool aHeaderOnly )
{
    Read( aDoc, aHeaderOnly );
}

void IDX_HOLDER::Read( wxXmlDocument& aDoc, bool aHeaderOnly )
{
    std::shared_ptr<IDX_XML_NODE> child = std::make_shared<IDX_XML_NODE_KICAD>( aDoc.GetRoot() );

    std::cout << "Trying to read IDX file...";
    if( aHeaderOnly )
        std::cout << "header" << std::endl;
    else
        std::cout << "full" << std::endl;

    m_dataset = std::make_shared<idx_foundation::EDMD_DATASET>( *child, aHeaderOnly );
    m_dataset->Resolve( m_dataset->m_body.m_registry );

    for( const std::shared_ptr<idx_pdm::EDMD_ITEM>& i : m_dataset->m_body.m_items )
    {
        for( const std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>& inst : i->m_itemInstance )
        {
            if( auto type = inst->GetUserProperty( "TYPE" ) )
            {
                if( type->m_value == "BOARDOUTLINE" )
                {
                    m_dataset->m_body.m_boardOutline.push_back( inst );
                    continue;
                }
            }

            auto interstratum = std::dynamic_pointer_cast<idx_pdm::EDMD_INTERSTRATUM_FEATURE>( inst->m_item->m_shape );
            if( interstratum )
            {
                using FT = idx_pdm::INTERSTRATUM_FEATURE_TYPE;
                switch( interstratum->m_interstratumFeatureType )
                {
                case FT::BOARD_CUTOUT: m_dataset->m_body.m_boardOutline.push_back( inst ); continue;
                case FT::CUTOUT: m_dataset->m_body.m_NPTH.push_back( inst ); continue;
                case FT::PLATED_CUTOUT: m_dataset->m_body.m_PTH.push_back( inst ); continue;
                default: break;
                }
            }

            if( inst->m_item && inst->m_item->m_shape )
            {
                auto keep = std::dynamic_pointer_cast<idx_pdm::EDMD_KEEP_CONSTRAINT>( inst->m_item->m_shape );
                if( keep )
                {
                    m_dataset->m_body.m_keepZones.push_back( inst );
                    continue;
                }
            }

            if( !inst->m_name.empty() )
            {
                m_dataset->m_body.m_components.push_back( inst );
                continue;
            }

            m_dataset->m_body.m_others.push_back( inst );
        }
    }
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetBoardOutlineElements() const
{
    return m_dataset->m_body.m_boardOutline;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetNPTH() const
{
    return m_dataset->m_body.m_NPTH;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetComponents() const
{
    return m_dataset->m_body.m_components;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetPTH() const
{
    return m_dataset->m_body.m_PTH;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetOthers() const
{
    return m_dataset->m_body.m_others;
}

std::vector<std::shared_ptr<idx_pdm::EDMD_ITEM_INSTANCE>> IDX_HOLDER::GetKeepZones() const
{
    return m_dataset->m_body.m_keepZones;
}
