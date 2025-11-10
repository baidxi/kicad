#include "CAD_software.h"

// ----------------- IDX_BASE -----------------
void IDX_BASE::Report(std::string aMsg, REPORT_LEVEL aLvl)
{
    wxString ws(aMsg.c_str(), wxConvUTF8);
    switch (aLvl)
    {
        case REPORT_LEVEL::INFO:
            std::cout << "INFO: ";
            break;
        case REPORT_LEVEL::ACTION:
            std::cout << "ACTION: ";
            break;
        case REPORT_LEVEL::WARNING:
            std::cout << "WARNING: ";
            break;
        case REPORT_LEVEL::ERROR:
            std::cout << "ERROR: ";
            break;
    }
    std::cout << aMsg << std::endl;
}

// ----------------- IDX_XML_NODE_KICAD -----------------
IDX_XML_NODE_KICAD::IDX_XML_NODE_KICAD(wxXmlNode* aNode)
    : m_node(aNode) {}

std::string IDX_XML_NODE_KICAD::GetName() const
{
    return m_node != nullptr ? m_node->GetName().ToStdString() : "nullptr";
}

std::string IDX_XML_NODE_KICAD::GetAttribute(const std::string& name, const std::string& defaultValue) const
{
    if (!m_node) return "";
    return m_node->GetAttribute(name, defaultValue).ToStdString();
}

void IDX_XML_NODE_KICAD::SetAttribute(const std::string& name, const std::string& value)
{
    if (m_node) m_node->AddAttribute(name, value);
}

std::string IDX_XML_NODE_KICAD::GetText() const
{
    if (!m_node) return "";
    wxXmlNode* child = m_node->GetChildren();
    if (child && child->GetType() == wxXML_TEXT_NODE)
        return child->GetContent().ToStdString();
    return "";
}

void IDX_XML_NODE_KICAD::SetText(const std::string& text)
{
    if (!m_node) return;
    wxXmlNode* child = m_node->GetChildren();
    if (child && child->GetType() == wxXML_TEXT_NODE)
        child->SetContent(text);
    else
        m_node->AddChild(new wxXmlNode(wxXML_TEXT_NODE, "", text));
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::GetParent() const
{
    return nullptr;
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::GetChildren() const
{
    if (!m_node) return nullptr;
    wxXmlNode* firstChild = m_node->GetChildren();
    if (!firstChild) return nullptr;
    return std::make_shared<IDX_XML_NODE_KICAD>(firstChild);
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::AppendChild(const std::string& name)
{
    if (!m_node) return nullptr;
    auto* newChild = new wxXmlNode(wxXML_ELEMENT_NODE, name);
    m_node->AddChild(newChild);
    return std::make_shared<IDX_XML_NODE_KICAD>(newChild);
}

void IDX_XML_NODE_KICAD::RemoveChild(const std::shared_ptr<IDX_XML_NODE>& child)
{
    auto wxChild = std::dynamic_pointer_cast<IDX_XML_NODE_KICAD>(child);
    if (m_node && wxChild && wxChild->m_node)
        m_node->RemoveChild(wxChild->m_node);
}

std::string IDX_XML_NODE_KICAD::GetNodeContent() const
{
    return m_node  != nullptr ? m_node->GetNodeContent().ToStdString() : "nullptr";
}

std::shared_ptr<IDX_XML_NODE> IDX_XML_NODE_KICAD::GetNext() const
{
    if (!m_node || !m_node->GetNext()) return nullptr;
    return std::make_shared<IDX_XML_NODE_KICAD>(m_node->GetNext());
}
