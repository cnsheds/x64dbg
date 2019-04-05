#include "RvaInfo.h"
#include <tchar.h>
#include "pugixml/pugixml.hpp"


using namespace pugi;

CRvaInfo::CRvaInfo()
{
}


CRvaInfo::~CRvaInfo()
{
}

bool CRvaInfo::AddRVAInfo(saddr rva, rstring moduleName, rstring rvaname)
{
    if(0 == rva || moduleName.length() == 0 || rvaname.length() == 0)
        return false;

    std::map<rstring, MAP_RVAINFO >::iterator it = m_mapRVA.find(moduleName.toLower());
    if(it != m_mapRVA.end())
    {
        it->second[rva] = rvaname;
    }
    else
    {
        MAP_RVAINFO locRVAInfo;
        locRVAInfo[rva] = rvaname;
        m_mapRVA[moduleName] = locRVAInfo;
    }

    return true;
}

rstring CRvaInfo::GetRVAname(saddr rva, rstring moduleName)
{
    if(0 == rva || moduleName.length() == 0)
        return rstring();

    std::map<rstring, MAP_RVAINFO >::iterator it = m_mapRVA.find(moduleName.toLower());
    if(it != m_mapRVA.end())
    {
        if(it->second.find(rva) != it->second.end())
            return it->second[rva];
    }
    return rstring();
}

int CRvaInfo::SaveRVAInfo(rstring filepath)
{
    xml_document doc;
    xml_node rootNode = doc.append_child(_T("MI"));
    for(auto & rvaitem : m_mapRVA)
    {
        xml_node module_node = rootNode.append_child(rvaitem.first.toStdString().c_str());
        for(auto & rva : rvaitem.second)
        {
            xml_node rva_node = module_node.append_child(_T("m"));
            TCHAR wsbuf[32] = { 0 };
            _stprintf_s(wsbuf, 32, _T("%X"), rva.first);
            rva_node.append_attribute(_T("rva")).set_value(wsbuf);
            rva_node.append_attribute(_T("n")).set_value(rva.second.toStdString().c_str());
        }
    }

    std::string sfilepath = filepath.toLocal8Bit().constData();
    return doc.save_file(sfilepath.c_str());
}

int CRvaInfo::LoadRVAInfo(rstring filepath)
{
    xml_document doc;
    std::string sfilepath = filepath.toLocal8Bit().constData();
    xml_parse_result result = doc.load_file(sfilepath.c_str());
    if(result.status != status_ok)
        return 1;

    xml_node rootNode = doc.child(_T("MI"));
    if(!rootNode)
        return 2;

    m_mapRVA.clear();
    for(pugi::xml_node_iterator it = rootNode.begin(); it != rootNode.end(); ++it)
    {
        rstring moduleName = it->name();
        MAP_RVAINFO map_module;
        for(pugi::xml_node_iterator mit = it->begin(); mit != it->end(); ++mit)
        {
            rstring strrva = mit->attribute(_T("rva")).value();
            saddr rva = strrva.toUInt(nullptr, 16);
            rstring rvaname = mit->attribute(_T("n")).value();
            map_module[rva] = rvaname;
        }
        m_mapRVA[moduleName] = map_module;
    }

    return 0;
}
