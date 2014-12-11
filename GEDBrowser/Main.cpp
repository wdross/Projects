//---------------------------------------------------------------------------
#include <vcl.h>
#include <inifiles.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <registry.hpp>
#include <printers.hpp>
#include <jpeg.hpp>
#include <filectrl.hpp>
#pragma hdrstop

#define Tw 60 // ThumbnailWidth
#define Th 75 // ThumbnailHeight
#define Bw 512 // BigWidth
#define Bh 384 // BigHeight

#include "Main.h"
#include "ProgressUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TMainForm *MainForm;
InformationType Information;
TIniFile *Configuration;
AnsiString Section = "GEDBrowser";
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
    : TForm(Owner)
{
  CurrYear = Now().FormatString("yyyy").ToInt();
  Configuration = new TIniFile(".\\GEDBrowser.ini");

  Individuals = new TStringList;
  Families = new TStringList;

  GEDFile = new TStringList();
  GEDFile->LoadFromFile(Configuration->ReadString(Section, "File", ".\\GEDTest.GED"));

  Months = new TStringList();
  Months->CommaText = "JAN,FEB,MAR,APR,MAY,JUN,JUL,AUG,SEP,OCT,NOV,DEC";
  ShortDateFormat = "MM/DD/YYYY";

  ParseRelations();

  IndList->Items = Individuals;
  IndList->ItemIndex = 6; // pick a value
  IndListClick((TObject *)NULL); // Pretend to click on it

  FamList->Items = Families;
  FamList->ItemIndex = 1;
  FamListClick((TObject *)NULL);

  if (Now().FormatString("mm").ToInt() > 1)
    CurrYear++; // Must be working on next year

  Start->Date = "01/01/"+IntToStr(CurrYear);
  Stop->Date = "12/31/"+IntToStr(CurrYear);
}
//---------------------------------------------------------------------------
__fastcall TMainForm::~TMainForm()
{
  int i;
  delete GEDFile;

  // Dump all allocated Individuals
  for (i=0; i<Individuals->Count; i++)
    if (Individuals->Objects[i]) // Non-Null object entry
      delete (Individual*)Individuals->Objects[i];
  delete Individuals;

  // Dump all allocated Families
  for (i=0; i<Families->Count; i++)
    if (Families->Objects[i]) // Non-null object entry
      delete (Family*)Families->Objects[i];
  delete Families;
  delete Configuration;
  delete Months;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::CloseBtnClick(TObject *Sender)
{
  Close();
}
//---------------------------------------------------------------------------
bool TMainForm::SameLastMarriage(Individual *Indi, Individual **Spouse)
{
  Family *Fam = (Family*)Families->Objects[Indi->SpouseIn[Indi->SpouseIn[0]]];
  *Spouse = NULL;
  if (!Fam->Husband || !Fam->Wife || (Fam->Divorced.Date != ""))
    return false;
  if (Indi->Gender == 'M')
    *Spouse = (Individual*)Individuals->Objects[Fam->Wife];
  else
    *Spouse = (Individual*)Individuals->Objects[Fam->Husband];

  if (Indi->SpouseIn[Indi->SpouseIn[0]] == (*Spouse)->SpouseIn[(*Spouse)->SpouseIn[0]])
    return true;

  return false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ParseRelations()
{
  int i, j, k, l, Num, level;
  Individual *Indi = NULL;
  Family *Fam = NULL;
  FirmDate *DateReceiver;
  AnsiString *PlaceReceiver;
  TStringList **NoteReceiver, **PictureReceiver;
  char Processing;
  bool WatchTODO;

  AnsiString Line;
  // Take the text based data in GEDFile and parse it out into
  // Individuals and Families

  Num = NumIndividuals = NumFamilies = 0;
  i = 0; // Line counter
  while (i < GEDFile->Count) {
    level = GEDFile->Strings[i].SubString(1,1).ToInt();
    if (GEDFile->Strings[i].SubString(1,6) == "3 CONT")
      level = 2; // force code to accept level 3 as though 2, just for CONT entries
    else if (GEDFile->Strings[i].SubString(1,6) == "2 EMAI" ||
             GEDFile->Strings[i].SubString(1,6) == "2 _EMA" ||
             GEDFile->Strings[i].SubString(1,6) == "2 ADDR")
      level = 1; //  phone & Addr used to be level 1
    switch (level) {
      case 0:// See what level '0' item needs to be processed
        if (GEDFile->Strings[i].SubString(1,4) == "0 @I") { // Signature of new Individual
          Processing = 'I';
          Line = GEDFile->Strings[i].SubString(5,1024); // Get Number and "INDI"

          // Determine INDI unique number
          Num = Line.SubString(1,Line.Pos("@")-1).ToIntDef(0);
          while (Individuals->Count < Num) // Pad the list
            Individuals->Insert(Individuals->Count,"?");

          // Create and initialize storage for Individual information
          Indi = new Individual;
          memset(Indi,0,sizeof(*Indi));
          Indi->Number = Num;
          Indi->Gender = '?';
          Indi->BirthDate.Date = "";
          Indi->BirthDate.Questionable = true;

          Individuals->InsertObject(Num,"X",(TObject*)Indi);
          NumIndividuals++;
        } // New INDI
        else if (GEDFile->Strings[i].SubString(1,4) == "0 @F") { // Signature of new Family
          Processing = 'F';
          Line = GEDFile->Strings[i].SubString(5,1024); // Get Number and "FAM"

          // Determine FAM unique number
          Num = Line.SubString(1,Line.Pos("@")-1).ToIntDef(0);
          while (Families->Count < Num) // Pad the list
            Families->Insert(Families->Count,"?");

          // Create and initialize storage for Family information
          Fam = new Family;
          memset(Fam,0,sizeof(*Fam));
          Fam->Number = Num;

          Families->InsertObject(Num,"X",(TObject*)Fam);
          NumFamilies++;
        } // New Family
        NoteReceiver = NULL; // Any level '0' item will reset the active NOTE
        WatchTODO = false;
        break;
      case 1: // Object specific data
        Line = GEDFile->Strings[i].SubString(3,4); // Get data type
        // Individual tags
        if (Line == "NAME") {
          if (Num) {
            Line = GEDFile->Strings[i].SubString(8,1024);
            // Find Last name inside of '/'
            j = Line.Pos("/");
            for (l=Line.Length(); (l>0)&&(Line.SubString(l,1)!="/"); l--)
              ;
            l = l-j-1;
            if (l>0)
              Indi->Last = Line.SubString(j+1,l);
            k = Line.Pos('"');
            if (k) { // Find nick-name
              Indi->First = Line.SubString(k+1,1024);
              Indi->First.Delete(Indi->First.Pos('"'),1024);
            }
            else { // take given name
              Indi->First = Line.SubString(1,Line.Pos(" ")-1);
            }

            // Get rid of '/' around last names
            j = Line.Pos("/");
            Line.Delete(j,1);
            if (Line.SubString(j-1,1) != " ") // If there's not a space, add one
              Line.Insert(" ",j);
            if (Line.SubString(j-2,2) == "  ")
              Line.Delete(j-2,1);
            Line = Line.Delete(Line.Pos("/"),1);

            Individuals->Strings[Num] = Line;
          }
        }
        else if (Line == "OCCU") {
          if (Indi->Occupation == "")
            Indi->Occupation = GEDFile->Strings[i].SubString(8,1024);
        }
        else if (Line == "SEX ") {
          Indi->Gender = *GEDFile->Strings[i].SubString(7,1).c_str();
        }
        else if (Line == "BIRT") {
          DateReceiver = &Indi->BirthDate;
          PlaceReceiver = &Indi->BirthPlace;
        }
        else if (Line == "OBJE") { // look for "2 FORM JPEG/PCX/BMP", FILE TITLE
          if (Processing == 'I') {
            if (!Indi->PictureFileNames)
              Indi->PictureFileNames = new TStringList();
            PictureReceiver = &Indi->PictureFileNames;
          }
          else if (Processing == 'F') {
            if (!Fam->PictureFileNames)
              Fam->PictureFileNames = new TStringList();
            PictureReceiver = &Fam->PictureFileNames;
          }
        }
        else if (Line == "FAMC") { // Child to family @F___@ (multiple)
          Line = GEDFile->Strings[i].SubString(10,1024); // Get Number and "@"
          Indi->ChildIn[++Indi->ChildIn[0]] = Line.SubString(1,Line.Pos("@")-1).ToIntDef(0);
        }
        else if (Line == "FAMS") { // Spouse in family @F___@ (multiple)
          Line = GEDFile->Strings[i].SubString(10,1024); // Get Number and "@"
          Indi->SpouseIn[++Indi->SpouseIn[0]] = Line.SubString(1,Line.Pos("@")-1).ToIntDef(0);
        }
        else if (Line == "DEAT") {
          DateReceiver = &Indi->Deceased;
          PlaceReceiver = &Indi->DeceasedPlace;
        }
        else if (Line == "NOTE" &&                                   // look for possible notes
                 GEDFile->Strings[i].SubString(8,8) != "Private:") { // that are not private
          if (Processing == 'I' &&
              GEDFile->Strings[i].SubString(8,5) == "NOWEB") {
            // secret entry that this person doesn't want to be on the web pages
            // we'll handle this by removing their name and not printing any notes
            Individuals->Strings[Num] = "Person Removed";
            Indi->NoPrint = true;
          }
          else if (Processing == 'I' &&
                   Indi->NoPrint) {
            // already decided not to print, so don't gather subsiquent notes
          }
          else if (Processing == 'I') {
            Indi->Notes = new TStringList();
            Indi->Notes->Add(GEDFile->Strings[i].SubString(8,1024));
            NoteReceiver = &Indi->Notes; // Look for further lines
          }
          else if (Processing == 'F') {
            Fam->Notes = new TStringList();
            Fam->Notes->Add(GEDFile->Strings[i].SubString(8,1024));
            NoteReceiver = &Fam->Notes; // Look for further lines
          }
        }
        else if (Line == "ADDR") { // we have an address, family or individual
          if (Processing == 'I') {
            Indi->Address = new TStringList();
            NoteReceiver = &Indi->Address; // Look for further lines
          }
          else if (Processing == 'F') {
            Fam->Address = new TStringList();
            NoteReceiver = &Fam->Address; // Look for further lines
          }
          if (GEDFile->Strings[i].Length() > 7) // Something worthwhile
            (*NoteReceiver)->Add(GEDFile->Strings[i].SubString(8,1024));
        }
        else if (Line == "PHON") { // we have an address, family or individual
          if (Processing == 'I') {
            if (!Indi->Address)
              Indi->Address = new TStringList();
            else
              Indi->Address->Add("<BR>");
            Indi->Address->Add(GEDFile->Strings[i].SubString(8,1024));
          }
          else if (Processing == 'F') {
            if (!Fam->Address)
              Fam->Address = new TStringList();
            else
              Fam->Address->Add("<BR>");
            Fam->Address->Add(GEDFile->Strings[i].SubString(8,1024));
          }
        }
        else if (Line == "_EMA" ||
                 Line == "EMAI") { // we have an address, family or individual
          if (Processing == 'I')
            Indi->Email = GEDFile->Strings[i].SubString(9,1024).TrimLeft();
          else if (Processing == 'F')
            Fam->Email = GEDFile->Strings[i].SubString(9,1024).TrimLeft();
        }

        // Family tags
        else if (Line == "HUSB") { // individual @I___@
          Line = GEDFile->Strings[i].SubString(10,1024); // Get Number and "@"
          Fam->Husband = Line.SubString(1,Line.Pos("@")-1).ToIntDef(0);
        }
        else if (Line == "WIFE") { // individual @I___@
          Line = GEDFile->Strings[i].SubString(10,1024); // Get Number and "@"
          Fam->Wife = Line.SubString(1,Line.Pos("@")-1).ToIntDef(0);
        }
        else if (Line == "CHIL") { // individual @I___@ (multiple)
          Line = "";
          if (Fam->Husband) {
            Line = ((Individual*)(Individuals->Objects[Fam->Husband]))->First;
            if (Fam->Wife)
              Line = Line + " & ";
            else
              Line = Line + " " + ((Individual*)(Individuals->Objects[Fam->Husband]))->Last;
          }
          if (Fam->Wife) {
            Line = Line + ((Individual*)(Individuals->Objects[Fam->Wife]))->First + " ";
            if (Fam->Husband)
              Line = Line + ((Individual*)(Individuals->Objects[Fam->Husband]))->Last;
            else
              Line = Line + ((Individual*)(Individuals->Objects[Fam->Wife]))->Last;
          }
          Families->Strings[Num] = Line;

          Line = GEDFile->Strings[i].SubString(10,1024); // Get Number and "@"
          Fam->Child[Fam->Children++] = Line.SubString(1,Line.Pos("@")-1).ToIntDef(0);
        }
        else if (Line == "MARR") {
          DateReceiver = &Fam->Married;
          PlaceReceiver = &Fam->MarrPlace;
        }
        // DUPE TAG!!!!
        else if (Line == "OBJE") { // Family pictures handled above
        }
        else if (Line == "DIV ") { // Divorced
          Fam->Divorced.Date = "Yes"; // In case there is no date entry, it is at least not ""
          Fam->Divorced.Questionable = true;
          DateReceiver = &Fam->Divorced;
          PlaceReceiver = &Fam->DivPlace;
        }
        else if (Processing == 'I' && Line == "_TOD") { // TO DO
          // Set up to look for "DESC" "LastName:" and pick the more
          // desirable last name for this individual when it comes to Calendar...
          WatchTODO = true;
        }
        else if (Processing == 'F' && Line == "_NMR") { // Never married tag
          Fam->NeverMarried = true;
        }
        break;
      case 2: // Object specific data
        Line = GEDFile->Strings[i].SubString(3,4); // Get data type
        if (DateReceiver && (Line == "DATE")) {
          AnsiString Str, Year, Month;
          int m,d,y;
          try {
            Str = GEDFile->Strings[i].SubString(8,1024);
            DateReceiver->Date = Str; // as entered by user
            m = Str.Pos(" OR ");
            if (m > 0)
              Str.Delete(m,1024); // delete questionable portion

            int l = Str.LastDelimiter(" -");
            Year = Str.SubString(l+1,100);
            Str = Str.SubString(1,l-1);

            l = Str.LastDelimiter(" -");
            Month = Str.SubString(l+1,100);
            Str = Str.SubString(1,l-1);

            y = Year.ToIntDef(0);
            m = Months->IndexOf(Month.UpperCase())+1;
            d = Str.ToIntDef(0);
            DateReceiver->Questionable = true;
            if ((y != 0) && (m == 0) || (d == 0)) {
              if (!m)
                m = 1; // Pretend the 1st
              if (!d)
                d = 1;
            }
            else if ((y == 0) && (m != 0) && (d != 0)) {
              y = 1950; // Pretend it's some year...
            }
            else
              DateReceiver->Questionable = false;

            if ((y == 0) || (m == 0) || (d == 0)) {
              DateReceiver->TDate = 0;
              DateReceiver->Questionable = true;
            }
            else {
              DateReceiver->TDate = TDateTime((unsigned short)y,(unsigned short)m,(unsigned short)d);
//              if (DateReceiver->TDate < TDateTime(0)) {
//                DateReceiver->TDate = 0;
//                DateReceiver->Questionable = true;
//              }
            }
          }
          catch (...) {
            DateReceiver->TDate = 0;
            DateReceiver->Questionable = true;
          }

          DateReceiver = NULL; // Don't collect another
        }
        else if (PictureReceiver && (Line == "FORM") && GEDFile->Strings[i].SubString(8,1024) == "PDF") {
          PictureReceiver = NULL; // what's coming is not a picture
        }
        else if (PictureReceiver && (Line == "FILE")) {
          (*PictureReceiver)->Add(GEDFile->Strings[i].SubString(8,1024));

          PictureReceiver = NULL;
        }
        else if (PlaceReceiver && (Line == "PLAC")) {
          *PlaceReceiver = GEDFile->Strings[i].SubString(8,1024);
          PlaceReceiver = NULL;
        }
        else if (NoteReceiver && (Line == "ADDR" || Line == "CONT" || Line == "PHON" || Line == "EMAI")) {
          // Detect if the marker "Private:" was seen, then stop gathering
          // data to put on the web page -- Private means just that --
          // Don't share with the outside world!
          if (GEDFile->Strings[i].SubString(8,8) == "Private:")
            NoteReceiver = NULL;
          else
            (*NoteReceiver)->Add("<BR>"+GEDFile->Strings[i].SubString(8,1024));
        }
        else if (NoteReceiver && (Line == "CONC")) {
          (*NoteReceiver)->Strings[(*NoteReceiver)->Count-1] =
                         (*NoteReceiver)->Strings[(*NoteReceiver)->Count-1]+
                         GEDFile->Strings[i].SubString(8,1024);
        }
        else if (WatchTODO && (Line == "DESC")) {
          if (GEDFile->Strings[i].SubString(8,9).UpperCase() == "LASTNAME:")
            Indi->OverRideLast = GEDFile->Strings[i].SubString(17,1024);
          WatchTODO = false;
        }
        break;
      default:
        break;
    } // switch on record Type
    i++; // next line
  } // While more GED file to process

  // Go through all individuals, see if they will be printed with their spouse
  // or with their parent(s)
  Individual *Spouse;
  for (int ind=0; ind<Individuals->Count; ind++) {
    Indi = (Individual*)Individuals->Objects[ind];
    if (!Indi || Indi->PrintWithFam)
      continue; // Already has been assigned via earlier relation

    if (Indi->SpouseIn[0] && // Was married
        SameLastMarriage(Indi,&Spouse)) {
      Spouse->PrintWithFam = Indi->PrintWithFam = Indi->SpouseIn[Indi->SpouseIn[0]];
    }
    else { // Child has no "family" of his/her own.  Have stats show up with parents
      if (Indi->ChildIn[0] == 1)
        Indi->PrintWithFam = Indi->ChildIn[1];
      else if (Indi->ChildIn[0]) { // multiple parents...
        for (i=1; i<=Indi->ChildIn[0]; i++) {
          // if Fam was married when child was born, we'll assume birth parents
          Fam = (Family*)Families->Objects[Indi->ChildIn[i]];
try {
          if ((TDateTime(Fam->Married.TDate) < TDateTime(Indi->BirthDate.TDate)) &&
              (TDateTime(Indi->BirthDate.TDate) < TDateTime(Fam->Divorced.TDate))) {
            // This is the actual birthparents
            Indi->PrintWithFam = Fam->Number;
            break;
          }
}
catch (...) {
}
        }

        // look at each family that this child is part of, and see if the child was
        // born BEFORE that marriage, to assume that the later marriage would have
        // been a step or adoptive relationship

        if (!Indi->PrintWithFam && !Indi->NoPrint) {
          // Above code was unable to determine correct parents
          ShowMessage("Unmarried "+Individuals->Strings[Indi->Number]+" with multiple parents should be shown with which family?");
          Indi->PrintWithFam = Indi->ChildIn[1]; // birthparents are assumed to be first
        }
      }
    }
  }

  Output->Lines->Clear();
  Output->Lines->Add("Total of " + IntToStr(NumIndividuals) + " Individuals,");
  Output->Lines->Add("     and " + IntToStr(NumFamilies) + " Families found.");
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BadBirthClick(TObject *Sender)
{
  AnsiString Inter = BadBirth->Items->Strings[BadBirth->ItemIndex];
  IndList->ItemIndex = Inter.Delete(1,Inter.LastDelimiter("#")).ToIntDef(0);
  IndListClick(Sender);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ShowIndClick(TObject *Sender)
{
  if (static_cast<TLabel*>(Sender)->Tag) {
    IndList->ItemIndex = static_cast<TLabel*>(Sender)->Tag;
    IndListClick(Sender);
  }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::IndListClick(TObject *Sender)
{
  int j;
  TDateTime Test;

  Individual *Indi = (Individual*)Individuals->Objects[IndList->ItemIndex];
  Num->Caption = IndList->ItemIndex;
  if (Indi) {
    // Data exists
    CalendarPath->Text = ((Individual*)Individuals->Objects[IndList->ItemIndex])->Last;
    CreateRelated->Enabled = true;
    Gender->Caption = Indi->Gender;
    Birth->Caption = Indi->BirthDate.Date;
    Deceased->Caption = Indi->Deceased.Date;

    SpouseIn->Caption = "S: ";
    for (j=1; j<=Indi->SpouseIn[0]; j++) {
      SpouseIn->Caption = SpouseIn->Caption + " " + Indi->SpouseIn[j];
      SpouseIn->Tag = Indi->SpouseIn[j];
    }

    ChildIn->Caption = "C: ";
    for (j=1; j<=Indi->ChildIn[0]; j++) {
      ChildIn->Caption = ChildIn->Caption + " " + Indi->ChildIn[j];
      ChildIn->Tag = Indi->ChildIn[j];
    }
  }
  else {
    Gender->Caption = "";
    Birth->Caption = "";
    Deceased->Caption = "";
    SpouseIn->Caption = "";
    SpouseIn->Tag = 0;
    ChildIn->Caption = "";
    ChildIn->Tag = 0;
    CalendarPath->Text = "";
    CreateRelated->Enabled = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ShowFamClick(TObject *Sender)
{
  if (static_cast<TLabel*>(Sender)->Tag) {
    FamList->ItemIndex = static_cast<TLabel*>(Sender)->Tag;
    FamListClick(Sender);
  }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FamListClick(TObject *Sender)
{
  Family *Fam = (Family*)Families->Objects[FamList->ItemIndex];
  if (Fam) {
    Husband->Caption = Individuals->Strings[Fam->Husband];
    Husband->Tag = Fam->Husband;
    Wife->Caption = Individuals->Strings[Fam->Wife];
    Wife->Tag = Fam->Wife;
    Married->Caption = Fam->Married.Date;
  }
  else {
    Husband->Caption = "";
    Husband->Tag = 0;
    Wife->Caption = "";
    Wife->Tag = 0;
    Married->Caption = "";
  }
}
//---------------------------------------------------------------------------
int Total,WithPic;
int Added[10000];
TStringList *TheList;
AnsiString QuestionMark[2] = {"","?"};
int BumpBy;
void __fastcall TMainForm::AddName(Individual *Indi)
{
  if (Indi->NoPrint)
    return;
    
  AnsiString AddThis;
  if ((Indi->Deceased.Date == "") && (int(Indi->BirthDate.TDate) > 0)) {
    AddThis = Indi->First + " ";

    if (Indi->OverRideLast != "") // Male or Female, no matter what, use it
       AddThis += Indi->OverRideLast;
    else if (Indi->Gender == 'F' && Indi->SpouseIn[0]) {
      // Married woman at some point, traditionally use most recent husband's name, if not divorced
      Family *F = (Family*)Families->Objects[Indi->SpouseIn[Indi->SpouseIn[0]]];
      if ((F->Divorced.Date == "") && !(F->NeverMarried) && (F->Husband))
        AddThis += ((Individual*)Individuals->Objects[F->Husband])->Last;
      else
        AddThis += Indi->Last; // Use maiden name
    }
    else // use what I've got (Guys & unmarried Gals)
      AddThis += Indi->Last;

    AddThis = Indi->BirthDate.TDate.FormatString("mmddyyyy") +
              AddThis +
              " (" + QuestionMark[Indi->BirthDate.Questionable] + ")";

    Information.Fields.Picture = (Indi->PictureFileNames != NULL && Indi->PictureFileNames->Count > 0);
    WithPic += Information.Fields.Picture;
    Information.Fields.Type = INDIVIDUAL;
    Information.Fields.Number = Indi->Number;

    TheList->AddObject(AddThis,(TObject*)Information.AsInt);
  }
  else if (Indi->Deceased.Date == "") {
    BadBirth->Items->Add(Individuals->Strings[Indi->Number] + " #" + IntToStr(Indi->Number));
  }
  Added[Total++] = Indi->Number;   // Remember who was checked
} // AddName
//---------------------------------------------------------------------------
void __fastcall TMainForm::AddAnn(Family *Fam)
{
  if (!Fam->Husband || !Fam->Wife)
    return;

  Individual *Hus = (Individual*)(Individuals->Objects[Fam->Husband]);
  if (Hus->Deceased.Date != "")
    return;

  Individual *Wif = (Individual*)(Individuals->Objects[Fam->Wife]);
  if (Wif->Deceased.Date != "")
    return;

  if (int(Fam->Married.TDate) == 0) // (Fam->Married.Date == "N/A") || (Fam->Married.Date == ""))
    return;

  if (Hus->NoPrint || Wif->NoPrint)
    return;

  AnsiString Couple;
  if (Hus->OverRideLast != "" || Wif->OverRideLast != "") {
    if (Hus->OverRideLast != "")
      Couple = Hus->First + " " + Hus->OverRideLast;
    else
      Couple = Hus->First + " " + Hus->Last;
    Couple += " & " + Wif->First + " ";
    if (Wif->OverRideLast != "")
      Couple += Wif->OverRideLast;
    else
      Couple += Hus->Last;
  }
  else Couple = Hus->First +
                " & " +
                Wif->First + " " +
                Hus->Last;

  Couple = Fam->Married.TDate.FormatString("mmddyyyy") +
           Couple
           + " (" + QuestionMark[Fam->Married.Questionable] + ")";

  Information.Fields.Picture = (Fam->PictureFileNames != NULL && Fam->PictureFileNames->Count > 0);
  WithPic += Information.Fields.Picture;
  Information.Fields.Type = FAMILY;
  Information.Fields.Number = Fam->Number;
  TheList->AddObject(Couple,(TObject*)Information.AsInt);

  Added[Total++] = -Fam->Number;
} // AddAnn
//---------------------------------------------------------------------------
bool __fastcall AlreadyAdded(int Who)
{
  int k;
  for (k=0; k<Total; k++)
    if (Who == Added[k])
      break; // Found it!
  return (k < Total); // t=Found; f=not found
}
//---------------------------------------------------------------------------
void FinishPage(TStringList *HTML)
{
    HTML->Add("<HR NOSHADE SIZE=0><CENTER>");

    HTML->Add("<A HREF=\"../Calendars.html\"><B>Calendars</B></A> *");
//<A HREF="../WC_IDX/IDX001.htm"><B>Index</B></A> *
//<A HREF="../WC_IDX/SUR.htm"><B>Surnames</B></A> *
    HTML->Add("<A HREF=\"mailto:mr_trekkie@yahoo.com?subject=Family tree corrections\"><B>Send Wayne corrections</B></A> *");
    HTML->Add("<A HREF=\"../index.html\"><B>Home</B></A>");

    HTML->Add("</CENTER></BODY></HTML>");
} // FinishPage
//---------------------------------------------------------------------------
// Well, here it is.  The first recursive routine that I've had to write
// since CS102!!  Let's see how it goes...
// This is required to build the necessary output for creating a ancestor map,
// as the F)ather must be printed above (before) the s)elf is printed, after
// which the M)other must be printed (in turn, printing her father first...)
//---------------------------------------------------------------------------
int level; // keeps track of the indentation level (recursion level)
TStringList *Ancestors;
bool vline[100]; // is a vertical line present at each level?
void __fastcall TMainForm::PrintFsM(Individual *Indi)
{
  Family *Fam;
  level++;
  if (Indi->ChildIn[0]) {
    Fam = (Family*)Families->Objects[Indi->ChildIn[1]]; // assuming birth parents are listed first
    AddAnn(Fam);
  }
  else
    Fam = NULL;
  vline[level] = false;

  if (Fam && Fam->Husband)
    PrintFsM((Individual*)Individuals->Objects[Fam->Husband]);

  AddName(Indi);
  AnsiString Line = "";
  for (int j=1; j<level; j++) {
    Line += "    ";
    if (vline[j])
      Line += "|";
    else
      Line += " ";
  }
  Line += "<a href=\"../tree/Fam"+IntToStr(Indi->PrintWithFam)+".html\">"+Individuals->Strings[Indi->Number];
  if (Indi->PictureFileNames && Indi->PictureFileNames->Count > 0)
    Line += " <img BORDER=0 SRC=\"../camera.gif\">";
  Line += "</a>";
  Line += " (" + Indi->BirthDate.Date + " - " + Indi->Deceased.Date + ")";
  Ancestors->Add(Line);
  if (Indi->Gender == 'M')
    vline[level-1] = true; // this is a father, so show line to decendant
  else
    vline[level-1] = false; // this is a mother, so already have drawn line to decendant

  if (Fam && Fam->Wife)
    PrintFsM((Individual*)Individuals->Objects[Fam->Wife]);
  level--;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::CreateRelatedClick(TObject *Sender)
{
  int j, k, Go = 0;
  int GoList[1000];

  CreateRelated->Enabled = false;
  CreateRelated->Tag = IndList->ItemIndex; // save this for later

  Ancestors = new TStringList();

  Individual *Indi;
  Family *Fam;
  BadBirth->Items->Clear();

  if (TheList)
    TheList->Clear();
  else
    TheList = new TStringList();
  TheList->Sorted = true;
  if (!DirectoryExists(CalendarPath->Text))
    CreateDirectory(CalendarPath->Text.c_str(),NULL);

  Ancestors->Add("<html><head>");
  Ancestors->Add("<meta http-equiv=Content-Type content=\"text/html; charset=windows-1252\">");
  Ancestors->Add("<title>Ancestors of "+Individuals->Strings[IndList->ItemIndex]+"</title></head><body>");
  Ancestors->Add("<h3>Ancestors of "+Individuals->Strings[IndList->ItemIndex]+"</h3><pre>");

  Total = WithPic = 0;

  level = 0;
  vline[0] = false;
  // This will also populate the Added list, as it will AddName and AddAnn for all people/couples
  PrintFsM((Individual*)Individuals->Objects[IndList->ItemIndex]);

  // Go Down list of everyone added thus far
  memcpy(GoList,Added,sizeof(int)*Total);
  Go = Total;
  while (Go) {
    Indi = (Individual*)Individuals->Objects[abs(GoList[--Go])]; // Take one out of the list
    bool AddSpouse = (GoList[Go] > 0);

    if (!AlreadyAdded(Indi->Number))  // Need to add...
      AddName(Indi);

    for (k=1; k<=Indi->SpouseIn[0]; k++) {
      // In each family this person is a spouse of,
      Fam = (Family*)Families->Objects[Indi->SpouseIn[k]];
      // Add this person's children to the "go down" list:
      for (j=0; j<Fam->Children; j++) {
        if (!AlreadyAdded(Fam->Child[j]))
          GoList[Go++] = Fam->Child[j]; // add 'em all
      }
      if (AddSpouse &&                  // this isn't a spouse added by being a related spouse...
          (k == Indi->SpouseIn[0]) &&   // Most recent spouse
          (Fam->Divorced.Date == "")) { // No divorce indicated

        // if the other spouse's last marriage is this same one,
        // guess they are still together, so add their anniversary
        if (!AlreadyAdded(-Fam->Number))
          AddAnn(Fam);


        if (Indi->Gender == 'M') {
          if (Fam->Wife && !AlreadyAdded(Fam->Wife))
            GoList[Go++] = -Fam->Wife;     // Add current spouse to list
        }
        else {
          if (Fam->Husband && !AlreadyAdded(Fam->Husband))
            GoList[Go++] = -Fam->Husband;  // Add current spouse to list
        }
      }
    }
  }

  Output->Lines = TheList;
  Output->Lines->Add("Completed adding " + IntToStr(Output->Lines->Count) + " names and anniversaries");
  Output->Lines->Add(" (out of " + IntToStr(Total) + " checked) to the list,");
  Output->Lines->Add(" and " + IntToStr(WithPic) + " have pictures!");

  // Save the list of Birthdays and Anniversaries to BK data file for BK Calendar to see
  TheList->Sorted = false; // Don't rearrange the list now
  for (int i=0; i<TheList->Count; i++) {
    TheList->Strings[i] = TheList->Strings[i].SubString(9,1024) +
                          AnsiString().StringOfChar(' ',101-TheList->Strings[i].Length()+8) +
                          "1       " +
                          TheList->Strings[i].SubString(1,8);

  }
//  TheList->SaveToFile("BIRTH2.DTA");

  Ancestors->Add("</pre>");
  FinishPage(Ancestors);
  Ancestors->SaveToFile(CalendarPath->Text+"/Ancestors.html");
  delete Ancestors;

  HTMLCalClick(Sender); // Now run calendar too
  CreateRelated->Enabled = true;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::PrintPicturesClick(TObject *Sender)
{
  int i,toomuch;
  TRect Where;

  if (!TheList)
    return;

  TStringList *PicList = new TStringList();
  TPrinter *Prntr = Printer();
  Prntr->BeginDoc();

  int SideMargin = Prntr->PageWidth / 17;    // 1/2" outside margins
  int VertMargin = Prntr->PageHeight / 22;

//  for (Month=0; Month<1; Month++) {
 //   PicMonth->ItemIndex = Month;

    for (i=0; i<TheList->Count; i++) {
      if (TheList->Strings[i].SubString(110,2) == Start->Date.FormatString("mm")) {
        Information.AsInt = (int)TheList->Objects[i];
        if (Information.Fields.Picture) { // There is a picture present
          if (Information.Fields.Type == FAMILY)
            PicList->Add(((Family*)(Families->Objects[Information.Fields.Number]))->PictureFileNames->Strings[0]);
          else
            PicList->Add(((Individual*)(Individuals->Objects[Information.Fields.Number]))->PictureFileNames->Strings[0]);
        }
      }
    }

    try {
      if (PicList->Count > 0) {
        // Now that we have some pictures, let's arrange them on a sheet of paper...

        int PicsWide = 3;
        int PicsHigh = 3;
        while (PicsWide * PicsHigh < PicList->Count) {
          if ((PicsHigh % 2) == (PicsWide % 2))
            PicsWide++;
          else
            PicsHigh++;
        }

        // 1/4" spacing around pictures
        int AllowedWidth = (Prntr->PageWidth-SideMargin*2-(SideMargin*(PicsWide-1)/2))/PicsWide;
        int AllowedHeight =(Prntr->PageHeight-VertMargin*2-(VertMargin*(PicsHigh-1)/2))/PicsHigh;
        float ExpectedRatio = AllowedHeight/(float)AllowedWidth;

        int VerticalOffset = VertMargin; // At a minimum, top gets 1" margin
        // Split any unused height to center pictures vertically
        VerticalOffset += (PicsHigh - 1 - (PicList->Count-1)/PicsWide) *
                          AllowedHeight / 2;

        int HorizontalOffset = SideMargin; // At a minimum, left gets 1" margin
        // Split unused width to center pictures vertically
        if (PicList->Count < PicsWide)
          HorizontalOffset += (PicsWide - PicList->Count) * AllowedWidth / 2;


        for (i=0; i<PicList->Count; i++) {
          Graphics::TPicture *pic = new Graphics::TPicture();
          if (AtWork->Checked)
            PicList->Strings[i] = ExtractFileName(PicList->Strings[i]);
          pic->LoadFromFile(PicList->Strings[i]);

          Where.Left =   HorizontalOffset + (i%PicsWide)*(AllowedWidth+SideMargin/2);
          Where.Right =  Where.Left + AllowedWidth;
          Where.Top =    VerticalOffset + (i/PicsWide)*(AllowedHeight+VertMargin/2);
          Where.Bottom = Where.Top + AllowedHeight;

          float ThisRatio = pic->Height/(float)pic->Width;
          if (ThisRatio > ExpectedRatio) {
            // Picture is taller than wider, so scale appropriately,
            toomuch = AllowedWidth - (pic->Width * AllowedHeight / pic->Height);
            // move over left to center it,
            Where.Left += toomuch/2;
            // make Where indicate a narrower picture
            Where.Right -= toomuch/2;
          }
          else {
            // Picture is wider than taller, so scale,
            toomuch = AllowedHeight - (pic->Height * AllowedWidth / pic->Width);
            // move down to center it,
            Where.Top += toomuch/2;
            // make Where indicate a shorter picture
            Where.Bottom -= toomuch/2;
          }

          Prntr->Canvas->StretchDraw(Where,pic->Graphic);
          delete pic;
        }

      }
    }
    catch (...) {
      ShowMessage("Oopsis!");
    }
    Pictures->Lines = PicList;
    PicList->Clear();
//    if (Month < 11)
//      Prntr->NewPage();
//  } // For each month

  Prntr->EndDoc();
  delete PicList;
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::HTMLCalClick(TObject *Sender)
{
  Individual *Ind;
  int s,i,k,l,y,Where,li,Checking,PutNextMonthOnThisLine;
  bool ThisDay, done;
  AnsiString Person,PrevMonth;
  TDateTime Day;
  unsigned short year, month, day, smonth, tmonth, prevyear = 0;

  if (!TheList)
    return;

  TStringList *HTML = new TStringList();
  TStringList *Proto = new TStringList();
  Proto->LoadFromFile("MonthShell.html");
  TStringList *Index = new TStringList();

  Ind = (Individual*)Individuals->Objects[CreateRelated->Tag];
  Index->Add("<h3>Birthdays & Anniversaries of those related to <a href=\"" +
              CalendarPath->Text + "/Ancestors.html\">");
  Index->Add(Individuals->Strings[CreateRelated->Tag]+"</a></h3>");

  if (!DirectoryExists(CalendarPath->Text))
    CreateDirectory(CalendarPath->Text.c_str(),NULL);

  Stop->Date.DecodeDate(&year, &smonth, &day);
  Start->Date.DecodeDate(&year, &month, &day);

  do
  {
    Day = IntToStr(month)+"/01/"+year;
    AnsiString ReplaceHere = FormatDateTime("mmmm yyyy",Day);
    tmonth = month;
    if (year != prevyear) {
      Index->Add("<h1><b>" + IntToStr(year) + "</b></h1>");
      prevyear = year;
    }

    // Copy the ProtoType file header
    // Replace 'MonthHere' with the current month's information,
    // plus the first two rows (Month & days of the weeks)
    s=0;
    i=0; // number of "MonthHere" replacements
    while ((i<2) && (s<Proto->Count)) {
      l = HTML->Add(Proto->Strings[s]);
      while ((Where = HTML->Strings[l].Pos("MonthHere")) > 0) {
        // Replace with current information
        HTML->Strings[l] = HTML->Strings[l].Delete(Where,9);
        HTML->Strings[l] = HTML->Strings[l].Insert(ReplaceHere,Where);
        i++;
      }
      s++;
    }

    // imbed a table in the row on the calendar that has prev/home/next links
    HTML->Add(" <table border=0 width=100%>");
    HTML->Add(" <tr valign=center>");
    HTML->Add("  <td width=28% align=left>");
    // Previous month, if any
    if (PrevMonth != "") {
      HTML->Add("  <font size=-1>");
      HTML->Add("  <a href=\"" + PrevMonth + ".html\">&lt;&lt; "+PrevMonth+"</a>");
    }
    else
      HTML->Add("  <p>&nbsp;</p>");
    HTML->Add("  </td>"); // End of cell
    PrevMonth = ReplaceHere; // Remember month name for next time around

    // Center cell contains link information
    HTML->Add("  <td width=44% align=center>");
    HTML->Add("  <font size=-1>");
    HTML->Add("  <a href=\"http://WayneRossIs.BoldlyGoingNoWhere.org\">Ross home page</a>");
    HTML->Add("  <br><a href=\""+ReplaceHere+"plain.html\">Plain Printable version of this month</a>");
//    HTML->Add("  <br><a href=\"../tree/map.html\">Family tree</a>");
    HTML->Add("  </td>"); // End of cell

    HTML->Add("  <td width=28% align=right>");
    // Next month, if any will go here
    PutNextMonthOnThisLine = HTML->Count;
    HTML->Add("  <p>&nbsp;</p>"); // This placeholder will stay if no month is placed

    HTML->Add("  </td>"); // End of cell

    HTML->Add(" </tr></table>"); // End of row & table


    // Get rest of calendar header (Sun..Sat headers)
    i=0; // number of </tr> "End Rows" found
    while ((i<2) && (s<Proto->Count)) {
      l = HTML->Add(Proto->Strings[s]);
      while ((Where = HTML->Strings[l].Pos("MonthHere")) > 0) {
        // Replace with current information
        HTML->Strings[l] = HTML->Strings[l].Delete(Where,9);
        HTML->Strings[l] = HTML->Strings[l].Insert(ReplaceHere,Where);
      }
      if (HTML->Strings[l].Pos("</tr>") > 0)
        i++;
      s++;
    }



           // gives 1 2 3 4 5 6 7
    Day = Day - (Day.DayOfWeek() - 1); // now we are on the sunday before the end of month

    li=0; // List index
    done = false;
    while (!done) {
      HTML->Add("");
      HTML->Add("  <tr height=120 align=right valign=top>");
      Day.DecodeDate(&year, &month, &day);
      for (i=0; i<7; i++) { // Go across the days of the week, Sun..Sat
        if (month == tmonth) { // We'll print the day of the week
          HTML->Add("  <td>");
          HTML->Add("  <p>"+IntToStr(day));
          ThisDay = false;

          for ( ; li<TheList->Count; li++) {
            Checking = TheList->Strings[li].SubString(110,2).ToInt();
            if (Checking > tmonth) { // Once the Month has been passed, we'll quit
              li = TheList->Count;
              break;
            }
            if (Checking == tmonth) {
              Checking = TheList->Strings[li].SubString(112,2).ToInt();
              if (Checking > day) // we haven't got to this day yet...
                break;
              // Now we have the next person needing to be printed
              // on the calendar.
              Person = TheList->Strings[li].SubString(1,100).Trim();

              // Determine how old this person is (or long these people have been married)
              k = Person.Length()-1;
              if (Person.SubString(k,1) == "?")
                k--; // keep the ? if present
              while (Person.SubString(k,1) != "(") {
                Person.Delete(k,1);
                k--;
              }
              y = year - TheList->Strings[li].SubString(114,4).ToInt();
              Person.Insert(IntToStr(y),k+1);

              if (!ThisDay) {
                HTML->Add("  </p><font size=-1><p align=left>");
                ThisDay=true;
              }
              else
                HTML->Add("  <br>");

              Information.AsInt = (int)TheList->Objects[li];
              if (Information.Fields.Type == FAMILY) {
                HTML->Add("<a href=\"../tree/Fam" +
                          IntToStr(Information.Fields.Number) +
                          ".html\">" + Person + "</a>");
              }
              else {
                Ind = (Individual*)Individuals->Objects[Information.Fields.Number];
                if (Ind->PrintWithFam)
                  HTML->Add("<a href=\"../tree/Fam" +
                            IntToStr(Ind->PrintWithFam) +
                            ".html\">" + Person + "</a>");
                else
                  HTML->Add(Person);
              }
            }
          }
          HTML->Add("  </p>");
          HTML->Add("  </td>");
        }
        else { // blank cell
          HTML->Add("  <td>");
          HTML->Add("  <p>&nbsp;</p>");
          HTML->Add("  </td>");
        }
        Day++;
        Day.DecodeDate(&year, &month, &day);
        if ((i == 6) && (month != tmonth))
          done = true;
      }
      HTML->Add("</tr>");
      HTML->Add("");
    } // while we haven't printed the entire calendar

    // finish copying any prototype information onto output
    while (s<Proto->Count) {
      HTML->Add(Proto->Strings[s]);
      s++;
    }

    FinishPage(HTML);

    // Remember where the "Next month" link belongs and update if necessary
    if ((Day <= Stop->Date) || (tmonth != smonth)) {
      Day = IntToStr(month)+"/01/"+year;
      AnsiString ReplaceHere = FormatDateTime("mmmm yyyy",Day);
      HTML->Strings[PutNextMonthOnThisLine] = "  <font size=-1><a href=\"" + ReplaceHere + ".html\">"+ReplaceHere+" &gt;&gt;</a>";
    }

    HTML->SaveToFile(CalendarPath->Text+"\\"+ReplaceHere+".html");

    // Now we'll take this calendar and remove all the people link information,
    // and turn around and save that to a "plain" version
    for (s=0; s<HTML->Count; s++) {
      if (HTML->Strings[s].Pos("plain.html")) {
        HTML->Strings[s] = HTML->Strings[s].Delete(HTML->Strings[s].Pos("plain.html"),5);
        Where = HTML->Strings[s].Pos("Plain Printable");
        HTML->Strings[s] = HTML->Strings[s].Delete(Where,15).Insert("HTML",Where);
        //Delete(s); // don't show link to self
        //s--; // gotta look at the next line that will be moved up here
      }
      else { // keeping line, see if any changes need to be made
        Where = HTML->Strings[s].Pos("<a href=\"../tree/Fam");
        if (Where) {
          // found a link to be removed
          i=Where+20; // skip most of the found string
          while (HTML->Strings[s].SubString(i,1) != ">")
            i++;
          HTML->Strings[s] = HTML->Strings[s].Delete(Where,i-Where+1); // remove start marker
          Where = HTML->Strings[s].Pos("<");
          i = Where;
          while (HTML->Strings[s].SubString(i,1) != ">")
            i++;
          HTML->Strings[s] = HTML->Strings[s].Delete(Where,i-Where+1); // remove end marker
        }
        Where = HTML->Strings[s].Pos(".html");
        if (Where) {
          // Any links left, check if they are a month, point to "plain" version
          if (HTML->Strings[s].SubString(Where-4,4).ToIntDef(-1) > 0)
            // valid year, so point to "plain"
            HTML->Strings[s] = HTML->Strings[s].Insert("plain",Where);
        }
      } // keeping line
    }
    HTML->SaveToFile(CalendarPath->Text+"\\"+ReplaceHere+"plain.html");

    HTML->Clear();

    // Make an entry in the Index, so it'll have a link
    Index->Add("<a href=\"" +CalendarPath->Text+"/"+ReplaceHere+ ".html\">" +ReplaceHere+ "</a><br>");

  } while ((Day <= Stop->Date) || (tmonth != smonth));

  Index->Add("<br>");
  Index->SaveToFile(CalendarPath->Text+"\\Index.html");

  delete HTML;
  delete Proto;
  delete Index;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::StartExit(TObject *Sender)
{
  if (Start->Date > Stop->Date)
    Stop->Date = Start->Date;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::StopExit(TObject *Sender)
{
  if (Start->Date > Stop->Date)
    Start->Date = Stop->Date;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
TPicture *OrigPic;
TImage *iBMP;
TJPEGImage *ThumbNail;
// Regardless of source, this utility will always produce a .jpg output
void __fastcall TMainForm::MakeAnImage(AnsiString InputName,
                                       int AllowedWidth,
                                       int AllowedHeight,
                                       AnsiString OutputName,
                                       AnsiString &WidthNHeight)
{
  TRect Where;
  OrigPic->LoadFromFile(InputName);

  int toomuch;
  float ExpectedRatio = AllowedHeight/(float)AllowedWidth;

  float ThisRatio = OrigPic->Height/(float)OrigPic->Width;
  if (ThisRatio > ExpectedRatio) { // Picture is taller than wider, so scale
    toomuch = AllowedWidth - (OrigPic->Width * AllowedHeight / OrigPic->Height);
    // Cut how wide it is
    AllowedWidth -= toomuch;
    WidthNHeight = "HEIGHT="+IntToStr(Th);
  }
  else {                          // Picture is wider than taller, so scale
    toomuch = AllowedHeight - (OrigPic->Height * AllowedWidth / OrigPic->Width);
    // make a shorter picture
    AllowedHeight -= toomuch;
    WidthNHeight = "WIDTH="+IntToStr(Tw);
  }

  Where.Left = 0;
  Where.Top = 0;
  if (OrigPic->Width < AllowedWidth) { // We are trying to blow the picture up,
    // just leave everything alone by copying to iBMP
    Where.Bottom = OrigPic->Height;
    Where.Right = OrigPic->Width;
  }
  else { // Scale the picture down
    Where.Bottom = AllowedHeight;
    Where.Right = AllowedWidth;
  }

  iBMP->Width = Where.Right;
  iBMP->Height = Where.Bottom;
  iBMP->Picture = NULL;
  iBMP->Canvas->StretchDraw(Where,OrigPic->Graphic); // Create a BMP the correct size

  ThumbNail->Assign(iBMP->Picture->Bitmap);
  ThumbNail->SaveToFile(OutputName+".jpg");
} // MakeAnImage
TStringList *Addresses;
//---------------------------------------------------------------------------
void __fastcall TMainForm::IndDetails(Individual *Ind, TStringList **HTML, int fam)
{
  if (!Ind)
    return;

  AnsiString Pair;

  Pair = Ind->BirthDate.Date +", "+ Ind->BirthPlace;
  if (Pair != ", ")
    (*HTML)->Add("<B>Born</B> <FONT SIZE=-1>"+Pair+"</FONT>");
  if (Ind->Occupation != "")
    (*HTML)->Add("<BR><B>Occupation</B> <FONT SIZE=-1>"+Ind->Occupation+"</FONT>");
  Pair = Ind->Deceased.Date +", "+ Ind->DeceasedPlace;
  if (Pair != ", ")
    (*HTML)->Add("<BR><B>Died</B> <FONT SIZE=-1>"+Pair+"</FONT>");
  if (Ind->Notes) {
    (*HTML)->Add("<BR><B>Notes</B><FONT SIZE=-1>");
    (*HTML)->AddStrings(Ind->Notes);
    (*HTML)->Add("</FONT>");
  }

  if ((Ind->PrintWithFam == fam) && (Ind->Address || (Ind->Email != ""))) {
    (*HTML)->Add("<table border=0>");
    if (Ind->Address) {
      (*HTML)->Add("<tr><td align=left valign=top width=60><B>Address</B></td><td align=left valign=top width=180>");
      (*HTML)->Add("<FONT SIZE=-1>");
      (*HTML)->AddStrings(Ind->Address);
      (*HTML)->Add("</FONT></td></tr>");

      Addresses->Add("<a href=\"Fam"+IntToStr(fam)+".html\">");
      Addresses->AddStrings(Ind->Address);
      Addresses->Add("</a>");
    }
    else
      Addresses->Add(Individuals->Strings[Ind->Number]);
    if (Ind->Email != "") {
      (*HTML)->Add("<tr><td align=left valign=top width=60><B>Email</B></td><td align=left valign=top width=180>");
      (*HTML)->Add("<FONT SIZE=-1><A HREF=\"mailto:" + Ind->Email + "\"><B>" + Ind->Email + "</B></A></FONT></td></tr>");
      Addresses->Add("<br><A HREF=\"mailto:" + Ind->Email + "\"><B>" + Ind->Email + "</B></A>");
    }
    Addresses->Add("<br><br>");
    (*HTML)->Add("</table>");
  }


  int q;
  if (Ind->SpouseIn[0]) { // been married
    (*HTML)->Add("<TABLE BORDER=0><TR><TD WIDTH=50%>");
    for (q=Ind->SpouseIn[0]; (q>=1)&&(Ind->SpouseIn[q]!=fam); q--)
      ;
    // q should be the current marriage we are displaying...
    if (!q) // ran off the beginning of the list, we are a kid being printed with parents, so
      q = Ind->SpouseIn[0]+1; // pretend they were married again, so the last divorce shows up as a "previous"
    if (q>1) // there is a previous marriage to mention here
      (*HTML)->Add("<A HREF=\"Fam"+IntToStr(Ind->SpouseIn[q-1])+".html\">Previous Relation</A>");
    (*HTML)->Add("</TD><TD WIDTH=50%>");
    if (q<Ind->SpouseIn[0])
      (*HTML)->Add("<A HREF=\"Fam"+IntToStr(Ind->SpouseIn[q+1])+".html\">Next Relation</A>");
    (*HTML)->Add("</TD></TR></TABLE>");
  }
} // IndDetails
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
void __fastcall TMainForm::CreateTreeClick(TObject *Sender)
{
  CreateTree->Enabled = false; // cannot click again
  CancelTree = false;

  TStringList *HTML = new TStringList();
  OrigPic = new TPicture();
  iBMP = new TImage(this);
  ThumbNail = new TJPEGImage();
  Addresses = new TStringList();
  Addresses->Add("<h1>Address list</h1>");
  Individual *Hus, *Wif, *PMother, *PFather, *MMother, *MFather, *Child;
  Family *Fam, *Parents;
  AnsiString Pair;
  int SecondHusFam, SecondWifFam;

  if (!DirectoryExists("tree"))
    CreateDirectory("tree",NULL);

  // Put up progress bar screen, with cancel button
  Progress->Bar->Max = Families->Count;
  Progress->Bar->Min = 1;
  Progress->Show(); // User may cancel -- will set CancelTree to true

  for (int fam=1; (!CancelTree)&&(fam<Families->Count); fam++) {
    Fam = (Family*)Families->Objects[fam];
    if (!Fam)
      continue;

    if (fam % 5 == 1) {
      Progress->Bar->Position = fam; // show where we are in the scheme of things
      Progress->Bar->Invalidate();
      Application->ProcessMessages(); // allow update of the screen, and clicking "Cancel"
    }

    HTML->Clear();
    HTML->Add("<HTML><HEAD><TITLE></TITLE></HEAD>");
    HTML->Add("<BODY TEXT=\"#000000\">");
    HTML->Add("<CENTER>");

    HTML->Add("<TABLE BORDER=2 CELLPADDING=2>");
    HTML->Add("<TR><TD ALIGN=CENTER WIDTH=255>");

    // Find the parents of the Husband & Wife...
    PMother = PFather = MMother = MFather = Hus = Wif = NULL;
    SecondWifFam = SecondHusFam = 0;

    // Parents of the Wife
    if (Fam->Wife)
      Wif = (Individual*)Individuals->Objects[Fam->Wife];
    if (Wif && Wif->ChildIn[0]) {
      Parents = (Family*)Families->Objects[Wif->ChildIn[1]]; // Wife's primary family entry
      if (Parents->Wife)
        MMother = (Individual*)Individuals->Objects[Parents->Wife];
      if (Parents->Husband)
        MFather = (Individual*)Individuals->Objects[Parents->Husband];

      if (Wif->ChildIn[0] > 1)
        SecondWifFam = Wif->ChildIn[2]; // will make link later...
    }

    // Parents of the Husband
    if (Fam->Husband)
      Hus = (Individual*)Individuals->Objects[Fam->Husband];
    if (Hus && Hus->ChildIn[0]) {
      Parents = (Family*)Families->Objects[Hus->ChildIn[1]]; // Husband's primary family entry
      if (Parents->Wife)
        PMother = (Individual*)Individuals->Objects[Parents->Wife];
      if (Parents->Husband)
        PFather = (Individual*)Individuals->Objects[Parents->Husband];

      if (Hus->ChildIn[0] > 1)
        SecondHusFam = Hus->ChildIn[2]; // Brother's Keeper only allows 2...
    }

#define JUSTYEAR(a) (int(a.TDate)==0||a.Date==""?AnsiString(""):a.TDate.FormatString("yyyy"))
    // Put parents of each spouse at top of screen
    if (PFather) {
      HTML->Add("<B><A HREF=\"Fam"+IntToStr(Hus->ChildIn[1])+".html\">"+Individuals->Strings[PFather->Number]+"</A></B>");
      HTML->Add("<BR><font size=-1>(" +
                JUSTYEAR(PFather->BirthDate) + " - " +
                JUSTYEAR(PFather->Deceased) + ")</font>");
    }
    HTML->Add("</TD><TD ALIGN=CENTER WIDTH=255>");
    if (MFather) {
      HTML->Add("<B><A HREF=\"Fam"+IntToStr(Wif->ChildIn[1])+".html\">"+Individuals->Strings[MFather->Number]+"</A></B>");
      HTML->Add("<BR><font size=-1>(" +
                JUSTYEAR(MFather->BirthDate) + " - " +
                JUSTYEAR(MFather->Deceased) + ")</font>");
    }

    HTML->Add("</TD></TR><TR><TD ALIGN=CENTER WIDTH=255>");

    if (PMother) {
      HTML->Add("<B><A HREF=\"Fam"+IntToStr(Hus->ChildIn[1])+".html\">"+Individuals->Strings[PMother->Number]+"</A></B>");
      HTML->Add("<BR><font size=-1>(" +
                JUSTYEAR(PMother->BirthDate) + " - " +
                JUSTYEAR(PMother->Deceased) + ")</font>");
    }
    if (SecondHusFam)
      HTML->Add("<P ALIGN=LEFT><A HREF=\"Fam"+IntToStr(SecondHusFam)+".html\">Secondary Parents</A></P>");

    HTML->Add("</TD><TD ALIGN=CENTER WIDTH=255>");
    if (MMother) {
      HTML->Add("<B><A HREF=\"Fam"+IntToStr(Wif->ChildIn[1])+".html\">"+Individuals->Strings[MMother->Number]+"</A></B>");
      HTML->Add("<BR><font size=-1>(" +
                JUSTYEAR(MMother->BirthDate) + " - " +
                JUSTYEAR(MMother->Deceased) + ")</font>");
    }
    if (SecondWifFam)
      HTML->Add("<P ALIGN=RIGHT><A HREF=\"Fam"+IntToStr(SecondWifFam)+".html\">Secondary Parents</A></P>");
#undef JUSTYEAR

    HTML->Add("</TD></TR></TABLE><BR>");

    HTML->Add("<TABLE BORDER=4 CELLPADDING=4><TR><TD ALIGN=CENTER WIDTH=260>");
    if (Hus) {
      HTML->Add("<B><FONT COLOR=0000FF SIZE=+2>"+Individuals->Strings[Hus->Number]+"</FONT></B><BR>");
      if (Hus->PictureFileNames && Hus->PictureFileNames->Count > 0) {
        MakeAnImage(Hus->PictureFileNames->Strings[0],Bw,Bh,"tree\\Ind"+IntToStr(Hus->Number),Pair);
        HTML->Add("<A HREF=\"Ind"+IntToStr(Hus->Number)+".jpg\"><IMG BORDER=0 "+Pair+" ALIGN=MIDDLE SRC=\"Ind"+IntToStr(Hus->Number)+".jpg\"></A>");
      }
    }
    HTML->Add(" </TD><TD ALIGN=CENTER VALIGN=CENTER WIDTH=260>");

    if (Wif) {
      HTML->Add("<B><FONT COLOR=0000FF SIZE=+2>"+Individuals->Strings[Wif->Number]+"</FONT></B><BR>");
      if (Wif->PictureFileNames && Wif->PictureFileNames->Count > 0) {
        MakeAnImage(Wif->PictureFileNames->Strings[0],Bw,Bh,"tree\\Ind"+IntToStr(Wif->Number),Pair);
        HTML->Add("<A HREF=\"Ind"+IntToStr(Wif->Number)+".jpg\"><IMG BORDER=0 "+Pair+" ALIGN=MIDDLE SRC=\"Ind"+IntToStr(Wif->Number)+".jpg\"></A>");
      }
    }
    HTML->Add("</TD></TR></TABLE>");


    // output couple information
    HTML->Add("<TABLE BORDER=2 CELLPADDING=2><TR><TD ALIGN=CENTER COLSPAN=2>");
    Pair = Fam->Married.Date + ", " + Fam->MarrPlace;
    if (Pair != "")
      HTML->Add("<B>Married </B><FONT SIZE=-1>"+Pair+"</FONT>");
    Pair = Fam->Divorced.Date + ", " + Fam->DivPlace;
    if (Pair != ", ")
      HTML->Add("<BR><B>Divorced </B><Font SIZE=-1>"+Pair+"</FONT>");
    if (Fam->Notes) {
      HTML->Add("<BR><B>Note:</B><FONT SIZE=-1>");
      HTML->AddStrings(Fam->Notes);
      HTML->Add("</FONT>");
    }
    if (Fam->Address || (Fam->Email != "")) {
      HTML->Add("<table border=0>");
      if (Fam->Address) {
        HTML->Add("<tr><td align=left valign=top width=60><B>Address</B></td><td align=left valign=top width=180>");
        HTML->Add("<FONT SIZE=-1>");
        HTML->AddStrings(Fam->Address);
        HTML->Add("</a></FONT></td></tr>");
        if (Hus && Hus->Address) { // assume they are identical, remove
          delete Hus->Address;
          Hus->Address = NULL;
        }
        if (Wif && Wif->Address) { // assume they are identical, remove
          delete Wif->Address;
          Wif->Address = NULL;
        }

        Addresses->Add("<a href=\"Fam"+IntToStr(fam)+".html\">");
        Addresses->AddStrings(Fam->Address);
        Addresses->Add("</a>");
      }
      else
        Addresses->Add(Families->Strings[Fam->Number]);
      if (Fam->Email != "") {
        HTML->Add("<tr><td align=left valign=top width=60><B>Email</B></td><td align=left valign=top width=180>");
        HTML->Add("<FONT SIZE=-1><A HREF=\"mailto:" + Fam->Email + "\"><B>" + Fam->Email + "</B></A></FONT></td></tr>");
        Addresses->Add("<br><A HREF=\"mailto:" + Fam->Email + "\"><B>" + Fam->Email + "</B></A>");
        if (Hus && (Hus->Email == Fam->Email))
          Hus->Email = "";
        if (Wif && (Wif->Email == Fam->Email))
          Wif->Email = "";
      }
      Addresses->Add("<br><br>");
      HTML->Add("</table>");
    }

    HTML->Add("</TD></TR><TR VALIGN=TOP><TD WIDTH=266>");

    // output individual information
    IndDetails(Hus,&HTML,fam);

    HTML->Add("</TD><TD WIDTH=266>");

    IndDetails(Wif,&HTML,fam);

    HTML->Add("</TD></TR></TABLE>");


    if (Fam->Children) {
      HTML->Add("<BR><B>Children:</B><TABLE BORDER=2 CELLPADDING=2><TR><TD ALIGN=CENTER WIDTH=260>");
      for (int c=0; c<Fam->Children; c++) {
        Child = (Individual*)Individuals->Objects[Fam->Child[c]];
        if (Child->PrintWithFam &&
            Child->PrintWithFam != fam) {
          HTML->Add("<B><A HREF=\"Fam"+IntToStr(Child->PrintWithFam)+".html\">"+Individuals->Strings[Child->Number]+"</A></B><BR>");
        }
        else { // Child has no "family" of his/her own.  Make entry w/all stats
          HTML->Add("<FONT COLOR=0000FF><B>"+Individuals->Strings[Child->Number]+"</A></B></FONT>");
          if (Child->PictureFileNames &&  Child->PictureFileNames->Count > 0){
            MakeAnImage(Child->PictureFileNames->Strings[0],Bw,Bh,"tree\\Ind"+IntToStr(Child->Number),Pair);
            HTML->Add("<A HREF=\"Ind"+IntToStr(Child->Number)+".jpg\"><IMG BORDER=0 "+Pair+" ALIGN=MIDDLE SRC=\"Ind"+IntToStr(Child->Number)+".jpg\"></A>");
          }
          HTML->Add("<P align=left>");
          IndDetails(Child,&HTML,fam);
        }
        if (c < Fam->Children-1) // more coming up, make a cell
          HTML->Add("</TD></TR><TR><TD ALIGN=CENTER WIDTH=260>");
      }
      HTML->Add("</TD></TR></TABLE>");
    }
    HTML->Add("</CENTER>");

    FinishPage(HTML);

    HTML->SaveToFile("tree\\Fam"+IntToStr(fam)+".html");
  }
  Progress->Bar->Position = Progress->Bar->Max; // show where we are in the scheme of things
  Progress->Bar->Invalidate();
  Application->ProcessMessages(); // allow update of the screen

  delete HTML;
  delete OrigPic;
  delete ThumbNail;
  delete iBMP;

  Addresses->SaveToFile("tree\\Addresses.html");
  delete Addresses;

  Progress->Hide();

  CreateTree->Enabled = true;
  CreateTree->SetFocus();
}
//---------------------------------------------------------------------------

