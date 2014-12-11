//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "ProgressUnit.h"
#include "Main.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TProgress *Progress;
//---------------------------------------------------------------------------
__fastcall TProgress::TProgress(TComponent* Owner)
        : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TProgress::CancelBtnClick(TObject *Sender)
{
  CancelBtn->Enabled = false; // show user we got the message
  MainForm->CancelTree = true;
}
//---------------------------------------------------------------------------
