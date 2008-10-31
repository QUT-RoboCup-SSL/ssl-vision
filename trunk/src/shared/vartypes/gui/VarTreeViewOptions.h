//========================================================================
//  This software is free: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License Version 3,
//  as published by the Free Software Foundation.
//
//  This software is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  Version 3 in the file COPYING that came with this distribution.
//  If not, see <http://www.gnu.org/licenses/>.
//========================================================================
/*!
  \file    VarTreeViewOptions.h
  \brief   C++ Interface: VarTreeViewOptions
  \author  Stefan Zickler, (C) 2008
*/
#ifndef VARTREEVIEWOPTIONS_H_
#define VARTREEVIEWOPTIONS_H_
#include "VarTypes.h"

enum DT_GUI_COL_ENUM {
  DT_COL_NONE=0x00,
  DT_COL_TREE_NODE=0x01 << 0,
  DT_COL_TEXT_LABEL=0x01 << 1,
  DT_COL_ICON=0x01 << 2,
  DT_COL_TEXT_VALUE=0x01 << 3,
  DT_COL_EDITABLE=0x01 << 4,
  DT_COL_RANGEBARS=0x01 << 5,
};

typedef int VarColumnFlag;

/*!
  \class  VarTreeViewOptions
  \brief  An internal set of rendering parameters used by the VarTypes view-model.
  \author Stefan Zickler, (C) 2008
  \see    VarTypes.h

  This file is part of the QT4 Item-Model for VarTypes.
  It is used when you want to edit/visualize VarTypes using
  QT4's model/view system.

  If you don't know what VarTypes are, please see \c VarTypes.h 
*/
class VarTreeViewOptions
{
  protected:
      vector<VarColumnFlag> cols;
  public:
  void setColumns(vector<VarColumnFlag> v);
  vector<VarColumnFlag> getColumns() const;
  VarTreeViewOptions();
  virtual ~VarTreeViewOptions();

};


#endif /*VDATATYPETREEWIDGETITEM_H_*/
