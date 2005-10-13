/*
  The contents of this file are subject to the Initial Developer's Public
  License Version 1.0 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License here:
  http://www.flamerobin.org/license.html.

  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
  License for the specific language governing rights and limitations under
  the License.

  The Original Code is FlameRobin (TM).

  The Initial Developer of the Original Code is Milan Babuskov.

  Portions created by the original developer
  are Copyright (C) 2004 Milan Babuskov.

  All Rights Reserved.

  $Id$

  Contributor(s): Nando Dessena, Michael Hieke
*/

// For compilers that support precompilation, includes "wx/wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

// for all others, include the necessary headers (this file is usually all you
// need because it includes almost all "standard" wxWindows headers
#ifndef WX_PRECOMP
    #include "wx/wx.h"
#endif

#include "gui/FieldPropertiesDialog.h"
#include "metadata/column.h"
#include "metadata/database.h"
#include "ugly.h"
#include "urihandler.h"
//-----------------------------------------------------------------------------
FieldPropertiesDialog::FieldPropertiesDialog(wxWindow* parent, Table* table, 
        Column* column)
    : BaseDialog(parent, wxID_ANY, wxEmptyString)
{
    tableM = 0;
    columnM = 0;

    createControls();

    setTableM(table);
    setColumnM(column);

    layoutControls();
}
//-----------------------------------------------------------------------------
void FieldPropertiesDialog::createControls()
{
// TODO
}
//-----------------------------------------------------------------------------
void FieldPropertiesDialog::layoutControls()
{
// TODO
}
//-----------------------------------------------------------------------------
void FieldPropertiesDialog::setColumnM(Column* column)
{
    if (columnM != column)
    {
        if (columnM)
            columnM->detachObserver(this);
        columnM = column;
        if (columnM)
        {
            columnM->attachObserver(this);
            updateControlsFromColumn();
        }
    }
}
//-----------------------------------------------------------------------------
void FieldPropertiesDialog::setTableM(Table* table)
{
    if (tableM != table)
    {
        if (tableM)
            tableM->detachObserver(this);
        tableM = table;
        if (tableM)
        {
            tableM->attachObserver(this);
            updateControlsFromTable();
        }
    }
}
//-----------------------------------------------------------------------------
void FieldPropertiesDialog::updateControlsFromColumn()
{
// TODO
}
//-----------------------------------------------------------------------------
void FieldPropertiesDialog::updateControlsFromTable()
{
// TODO
}
//-----------------------------------------------------------------------------