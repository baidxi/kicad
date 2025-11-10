#pragma once

#include <idx/idx_base.h>
#include <wx/string.h>
#include <wx/xml/xml.h>
#include <iostream>
#include <memory>


class IDX_XML_NODE_KICAD : public IDX_XML_NODE
{
public:
    explicit IDX_XML_NODE_KICAD(wxXmlNode* node);

    std::string GetName() const override;
    std::string GetAttribute(const std::string& name, const std::string& defaultValue) const override;
    void SetAttribute(const std::string& name, const std::string& value) override;
    std::string GetText() const override;
    void SetText(const std::string& text) override;
    std::shared_ptr<IDX_XML_NODE> GetParent() const override;
    std::shared_ptr<IDX_XML_NODE> GetChildren() const override;
    std::shared_ptr<IDX_XML_NODE> AppendChild(const std::string& name) override;
    void RemoveChild(const std::shared_ptr<IDX_XML_NODE>& child) override;
    std::string GetNodeContent() const override;
    std::shared_ptr<IDX_XML_NODE> GetNext() const override;

private:
    wxXmlNode* m_node;
};
