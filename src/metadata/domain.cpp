/*
  Copyright (c) 2004-2016 The FlameRobin Development Team

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "core/FRError.h"
#include "core/ProgressIndicator.h"
#include "core/StringUtils.h"
#include "engine/MetadataLoader.h"
#include "frutils.h"
#include "metadata/database.h"
#include "metadata/domain.h"
#include "metadata/MetadataItemVisitor.h"
#include "sql/SqlTokenizer.h"

/*static*/
std::string Domain::getLoadStatement(bool list)
{
    std::string stmt("select "
            " f.rdb$field_name,"            //  1
            " f.rdb$field_type,"            //  2
            " f.rdb$field_sub_type,"        //  3
            " f.rdb$field_length,"          //  4
            " f.rdb$field_precision,"       //  5
            " f.rdb$field_scale,"           //  6
            " c.rdb$character_set_name,"    //  7
            " f.rdb$character_length,"      //  8
            " f.rdb$null_flag,"             //  9
            " f.rdb$default_source,"        // 10
            " l.rdb$collation_name,"        // 11
            " f.rdb$validation_source,"     // 12
            " f.rdb$computed_blr,"          // 13
            " c.rdb$bytes_per_character"    // 14
        " from rdb$fields f"
        " left outer join rdb$character_sets c"
            " on c.rdb$character_set_id = f.rdb$character_set_id"
        " left outer join rdb$collations l"
            " on l.rdb$collation_id = f.rdb$collation_id"
            " and l.rdb$character_set_id = f.rdb$character_set_id"
        " left outer join rdb$types t on f.rdb$field_type=t.rdb$type"
        " where t.rdb$field_name='RDB$FIELD_TYPE' and f.rdb$field_name ");
    if (list)
        stmt += "not starting with 'RDB$' order by 1";
    else
        stmt += "= ?";
    return stmt;
}

Domain::Domain(DatabasePtr database, const wxString& name)
    : MetadataItem((hasSystemPrefix(name) ? ntSysDomain : ntDomain),
        database.get(), name)
{
}

void Domain::loadProperties()
{
    setPropertiesLoaded(false);

    DatabasePtr db = getDatabase();
    MetadataLoader* loader = db->getMetadataLoader();
    MetadataLoaderTransaction tr(loader);
    wxMBConv* converter = db->getCharsetConverter();

    IBPP::Statement& st1 = loader->getStatement(getLoadStatement(false));
    st1->Set(1, wx2std(getName_(), converter));
    st1->Execute();
    if (!st1->Fetch())
        throw FRError(_("Domain not found: ") + getName_());

    loadProperties(st1, converter);
}

/*static*/
wxString Domain::trimDefaultValue(const wxString& value)
{
    // Some users reported two spaces before DEFAULT word in source
    // Also, equals sign is also allowed in newer FB versions
    // Trim(false) is trim-left
    wxString defValue(value);
    defValue.Trim(false);
    if (defValue.Upper().StartsWith("DEFAULT"))
        defValue.Remove(0, 7);
    else if (defValue.StartsWith("="))
        defValue.Remove(0, 1);
    defValue.Trim(false);
    return defValue;
}

void Domain::loadProperties(IBPP::Statement& statement, wxMBConv* converter)
{
    setPropertiesLoaded(false);

    statement->Get(2, &datatypeM);
    if (statement->IsNull(3))
        subtypeM = 0;
    else
        statement->Get(3, &subtypeM);

    // determine the (var)char field length
    // - system tables use field_len and char_len is null
    // - computed columns have field_len/bytes_per_char, char_len is 0
    // - view columns have field_len/bytes_per_char, char_len is null
    // - regular table columns and SP params have field_len/bytes_per_char
    //   they also have proper char_len, but we don't use it now
    statement->Get(4, &lengthM);
    int bpc = 0;   // bytes per char
    if (!statement->IsNull(14))
        statement->Get(14, &bpc);
    if (bpc && (!statement->IsNull(8) || !statement->IsNull(13)))
        lengthM /= bpc;

    if (statement->IsNull(5))
        precisionM = 0;
    else
        statement->Get(5, &precisionM);
    if (statement->IsNull(6))
        scaleM = 0;
    else
        statement->Get(6, &scaleM);
    if (statement->IsNull(7))
        charsetM = "";
    else
    {
        std::string s;
        statement->Get(7, s);
        charsetM = std2wxIdentifier(s, converter);
    }
    bool notNull = false;
    if (!statement->IsNull(9))
    {
        statement->Get(9, notNull);
    }
    nullableM = !notNull;
    hasDefaultM = !statement->IsNull(10);
    if (hasDefaultM)
    {
        readBlob(statement, 10, defaultM, converter);
        defaultM = trimDefaultValue(defaultM);
    }
    else
        defaultM = wxEmptyString;

    if (statement->IsNull(11))
        collationM = wxEmptyString;
    else
    {
        std::string s;
        statement->Get(11, s);
        collationM = std2wxIdentifier(s, converter);
    }
    readBlob(statement, 12, checkM, converter);

    setPropertiesLoaded(true);
}

bool Domain::isString()
{
    ensurePropertiesLoaded();
    return (datatypeM == 14 || datatypeM == 10 || datatypeM == 37);
}

bool Domain::isSystem() const
{
    wxString prefix(getName_().substr(0, 4));
    if (prefix == "MON$")
        return true;
    if (prefix != "RDB$")
        return false;
    long l;
    return getName_().Mid(4).ToLong(&l);    // numeric = system
}

//! returns column's datatype as human readable wxString.
wxString Domain::getDatatypeAsString()
{
    ensurePropertiesLoaded();
    return dataTypeToString(datatypeM, scaleM, precisionM, subtypeM, lengthM);
}

/* static*/
wxString Domain::dataTypeToString(short datatype, short scale, short precision,
    short subtype, short length)
{
    wxString retval;

    // special case (mess that some tools (ex. IBExpert) make by only
    // setting scale and not changing type)
    if (datatype == 27 && scale < 0)
    {
        retval = SqlTokenizer::getKeyword(kwNUMERIC);
        retval << "(15," << -scale << ")";
        return retval;
    }

    // INTEGER(prec=0), DECIMAL(sub_type=2), NUMERIC(sub_t=1), BIGINT(sub_t=0)
    if (datatype == 7 || datatype == 8 || datatype == 16)
    {
        if (scale == 0)
        {
            if (datatype == 7)
                return SqlTokenizer::getKeyword(kwSMALLINT);
            if (datatype == 8)
                return SqlTokenizer::getKeyword(kwINTEGER);
        }

        if (scale == 0 && subtype == 0)
            return SqlTokenizer::getKeyword(kwBIGINT);

        retval = SqlTokenizer::getKeyword(
            (subtype == 2) ? kwDECIMAL : kwNUMERIC);
        retval << "(";
        if (precision <= 0 || precision > 18)
            retval << 18;
        else
            retval << precision;
        retval << "," << -scale << ")";
        return retval;
    }

    switch (datatype)
    {
        case 10:
            return SqlTokenizer::getKeyword(kwFLOAT);
        case 27:
            return SqlTokenizer::getKeyword(kwDOUBLE) + " "
                + SqlTokenizer::getKeyword(kwPRECISION);

        case 12:
            return SqlTokenizer::getKeyword(kwDATE);
        case 13:
            return SqlTokenizer::getKeyword(kwTIME);
        case 35:
            return SqlTokenizer::getKeyword(kwTIMESTAMP);

        // add subtype for blob
        case 261:
            retval = SqlTokenizer::getKeyword(kwBLOB) + " "
                + SqlTokenizer::getKeyword(kwSUB_TYPE) + " ";
            retval << subtype;
            return retval;
            
        case 23: // Firebird v3
            return SqlTokenizer::getKeyword(kwBOOLEAN);

        // add length for char, varchar and cstring
        case 14:
            retval = SqlTokenizer::getKeyword(kwCHAR);
            break;
        case 37:
            retval = SqlTokenizer::getKeyword(kwVARCHAR);
            break;
        case 40:
            retval = SqlTokenizer::getKeyword(kwCSTRING);
            break;
    }
    retval << "(" << length << ")";
    return retval;
}

wxString Domain::getCollation()
{
    ensurePropertiesLoaded();
    return collationM;
}

wxString Domain::getCheckConstraint()
{
    ensurePropertiesLoaded();
    return checkM;
}

bool Domain::getDefault(wxString& value)
{
    ensurePropertiesLoaded();
    if (hasDefaultM)
    {
        value = defaultM;
        return true;
    }
    value = wxEmptyString;
    return false;
}

bool Domain::isNullable()
{
    ensurePropertiesLoaded();
    return nullableM;
}

void Domain::getDatatypeParts(wxString& type, wxString& size, wxString& scale)
{
    size = scale = wxEmptyString;
    wxString datatype = getDatatypeAsString();
    wxString::size_type p1 = datatype.find("(");
    if (p1 != wxString::npos)
    {
        type = datatype.substr(0, p1);
        wxString::size_type p2 = datatype.find(",");
        if (p2 == wxString::npos)
            p2 = datatype.find(")");
        else
        {
            wxString::size_type p3 = datatype.find(")");
            scale = datatype.substr(p2 + 1, p3 - p2 - 1);
        }
        size = datatype.substr(p1 + 1, p2 - p1 - 1);
    }
    else
    {
        type = datatype;
        // HACK ALERT: some better fix needed, but we don't want the subtype
        if (datatypeM == 261)
            type = SqlTokenizer::getKeyword(kwBLOB);
    }
}

wxString Domain::getCharset()
{
    ensurePropertiesLoaded();
    return charsetM;
}

wxString Domain::getAlterSqlTemplate() const
{
    return "ALTER DOMAIN " + getQuotedName() + "\n"
        "  SET DEFAULT { literal | NULL | USER }\n"
        "  | DROP DEFAULT\n"
        "  | ADD [CONSTRAINT] CHECK (condition)\n"
        "  | DROP CONSTRAINT\n"
        "  | new_name\n"
        "  | TYPE new_datatype;\n";
}

const wxString Domain::getTypeName() const
{
    return "DOMAIN";
}

void Domain::acceptVisitor(MetadataItemVisitor* visitor)
{
    visitor->visitDomain(*this);
}

// DomainCollectionBase
DomainCollectionBase::DomainCollectionBase(NodeType type,
        DatabasePtr database, const wxString& name)
    : MetadataCollection<Domain>(type, database, name)
{
}

DomainPtr DomainCollectionBase::getDomain(const wxString& name)
{
    DomainPtr domain = findByName(name);
    if (!domain)
    {
        SubjectLocker lock(this);

        DatabasePtr db = getDatabase();
        MetadataLoader* loader = db->getMetadataLoader();
        MetadataLoaderTransaction tr(loader);
        wxMBConv* converter = db->getCharsetConverter();

        IBPP::Statement& st1 = loader->getStatement(
            Domain::getLoadStatement(false));
        st1->Set(1, wx2std(name, converter));
        st1->Execute();
        if (st1->Fetch())
        {
            domain = insert(name);
            domain->loadProperties(st1, converter);
        }
    }
    return domain;
}

// Domains collection
Domains::Domains(DatabasePtr database)
    : DomainCollectionBase(ntDomains, database, _("Domains"))
{
}

void Domains::acceptVisitor(MetadataItemVisitor* visitor)
{
    visitor->visitDomains(*this);
}

void Domains::load(ProgressIndicator* progressIndicator)
{
    DatabasePtr db = getDatabase();
    MetadataLoader* loader = db->getMetadataLoader();
    MetadataLoaderTransaction tr(loader);
    wxMBConv* converter = db->getCharsetConverter();

    IBPP::Statement& st1 = loader->getStatement(
        Domain::getLoadStatement(true));

    CollectionType domains;
    st1->Execute();
    while (st1->Fetch())
    {
        checkProgressIndicatorCanceled(progressIndicator);
        if (!st1->IsNull(1))
        {
            std::string s;
            st1->Get(1, s);
            wxString name(std2wxIdentifier(s, converter));

            DomainPtr domain = findByName(name);
            if (!domain)
            {
                domain.reset(new Domain(db, name));
                initializeLockCount(domain, getLockCount());
            }
            domains.push_back(domain);
            domain->loadProperties(st1, converter);
            checkProgressIndicatorCanceled(progressIndicator);
        }
    }

    setItems(domains);
}

void Domains::loadChildren()
{
    load(0);
}

const wxString Domains::getTypeName() const
{
    return "DOMAIN_COLLECTION";
}

// System domains collection
SysDomains::SysDomains(DatabasePtr database)
    : DomainCollectionBase(ntSysDomains, database, _("System domains"))
{
}

void SysDomains::acceptVisitor(MetadataItemVisitor* visitor)
{
    visitor->visitSysDomains(*this);
}

const wxString SysDomains::getTypeName() const
{
    return "SYSDOMAIN_COLLECTION";
}

