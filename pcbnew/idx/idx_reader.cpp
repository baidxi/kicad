#include "idx_reader.h"

IDX_HOLDER::IDX_HOLDER( wxXmlDocument& aDoc, bool aHeaderOnly )
{
    Read( aDoc, aHeaderOnly );
}

void IDX_HOLDER::Read( wxXmlDocument& aDoc, bool aHeaderOnly )
{
    std::shared_ptr<IDX_XML_NODE> child = std::make_shared<IDX_XML_NODE_KICAD>( aDoc.GetRoot() );

    m_dataset = std::make_shared<idx_foundation::EDMD_DATASET>( *child, aHeaderOnly );
    m_dataset->Resolve();
    m_dataset->Prepare();
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
