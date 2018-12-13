#pragma once
#include <map>
#include <QString>

typedef unsigned long saddr;
typedef QString rstring;


class CRvaInfo
{
public:
    CRvaInfo();
    ~CRvaInfo();

    bool AddRVAInfo(saddr rva, rstring moduleName, rstring rvaname);
    rstring GetRVAname(saddr rva, rstring moduleName);

    //序列化RVA信息到文件
    int SaveRVAInfo(rstring filepath);
    int LoadRVAInfo(rstring filepath);

protected:
    typedef std::map<saddr, rstring> MAP_RVAINFO;
    std::map<rstring, MAP_RVAINFO > m_mapRVA;
};

