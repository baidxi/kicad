#include "idx_reader.h"

IDX_HOLDER::IDX_HOLDER( wxXmlDocument& aDoc, bool aHeaderOnly )
{
    Read( aDoc, aHeaderOnly );
}

void IDX_HOLDER::Read( wxXmlDocument& aDoc, bool aHeaderOnly )
{
    std::shared_ptr<IDX_XML_NODE> child = std::make_shared<IDX_XML_NODE_KICAD>( aDoc.GetRoot() );

    m_dataset = std::make_shared<idx_foundation::EDMD_DATA_SET>( *child, aHeaderOnly );
    m_dataset->ResolveAll();
    m_dataset->Prepare();
}
