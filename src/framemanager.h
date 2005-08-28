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

  The Initial Developer of the Original Code is Michael Hieke.

  Portions created by the original developer
  are Copyright (C) 2005 Michael Hieke.

  All Rights Reserved.

  Contributor(s): Milan Babuskov.
*/

#ifndef FR_FRAMEMANAGER_H
#define FR_FRAMEMANAGER_H

#include <wx/event.h>

#include <map>

#include "BaseFrame.h"
#include "gui/MetadataItemPropertiesFrame.h"
#include "metadata/metadataitem.h"
//---------------------------------------------------------------------------------------
class FrameAndId
{
public:
    BaseFrame *frame;
    int id;
    FrameAndId(BaseFrame *f, int i)
        :frame(f), id(i) {};
};
//---------------------------------------------------------------------------------------
typedef std::multimap<MetadataItem*, FrameAndId> ItemFrameMap;
//---------------------------------------------------------------------------------------
class FrameManager: public wxEvtHandler
{
public:
    FrameManager();
    ~FrameManager();

    void setWindowMenu(wxMenu *windowMenu);
    void rebuildMenu();
    void bringOnTop(int id);

    void removeFrame(BaseFrame* frame);
    MetadataItemPropertiesFrame* showMetadataPropertyFrame(wxWindow* parent, MetadataItem* item,
        bool delayed = false, bool force_new = false);

    void OnCommandEvent(wxCommandEvent& event);
protected:
    enum { ID_ACTIVATE_FRAME = 42 };
    DECLARE_EVENT_TABLE()
private:
    ItemFrameMap mipFramesM;
    wxMenu* windowMenuM;
    wxMenuBar* menuBarM;

    void removeFrame(BaseFrame* frame, ItemFrameMap& frames);
};

FrameManager& frameManager();
//---------------------------------------------------------------------------------------
#endif
