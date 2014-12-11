//---------------------------------------------------------------------------
#ifndef MainH
#define MainH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <Mask.hpp>
#include <ComCtrls.hpp>
#include <Grids.hpp>
#include <Menus.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
#define FAMILY 0
#define INDIVIDUAL 1
union InformationType {
  struct {
    unsigned int Picture:1;
    unsigned int Type:1;
    unsigned int Number:30;
  } Fields;
  unsigned int AsInt;
};
extern InformationType Information;
//---------------------------------------------------------------------------
struct FirmDate
{
  AnsiString Date;
  TDateTime  TDate;
  bool Questionable;
};
struct Individual
{
  int Number;
  char Gender;
  AnsiString First, Last, OverRideLast;
  FirmDate BirthDate;
  AnsiString BirthPlace;
  FirmDate Deceased;
  AnsiString DeceasedPlace;
  AnsiString Occupation;
  TStringList *PictureFileNames;
  int ChildIn[5]; // [0] will contain the number of entries
  int SpouseIn[5]; // [0] will contain the number of entries
  int PrintWithFam;
  bool NoPrint;
  TStringList *Notes;
  AnsiString Email;
  TStringList *Address;
};
struct Family
{
  int Number;
  int Husband;
  int Wife;
  int Children;
  int Child[40];
  FirmDate Married;
  FirmDate Divorced;
  bool NeverMarried;
  AnsiString MarrPlace;
  AnsiString DivPlace;
  TStringList *PictureFileNames;
  TStringList *Notes;
  AnsiString Email;
  TStringList *Address;
};
//---------------------------------------------------------------------------
class TMainForm : public TForm
{
__published:	// IDE-managed Components
    TButton *CloseBtn;
    TListBox *IndList;
    TListBox *FamList;
    TLabel *Birth;
    TLabel *Gender;
    TLabel *Deceased;
    TLabel *SpouseIn;
    TLabel *ChildIn;
    TLabel *Husband;
    TLabel *Wife;
    TLabel *Married;
    TLabel *Num;
    TButton *CreateRelated;
    TMemo *Output;
    TButton *PrintPictures;
    TMemo *Pictures;
    TCheckBox *AtWork;
    TDateTimePicker *Start;
    TDateTimePicker *Stop;
    TButton *CreateTree;
    TEdit *CalendarPath;
    TListBox *BadBirth;
    void __fastcall CloseBtnClick(TObject *Sender);
    void __fastcall IndListClick(TObject *Sender);
    void __fastcall ShowFamClick(TObject *Sender);
    void __fastcall FamListClick(TObject *Sender);
    void __fastcall CreateRelatedClick(TObject *Sender);
    void __fastcall PrintPicturesClick(TObject *Sender);
    void __fastcall HTMLCalClick(TObject *Sender);
    void __fastcall StartExit(TObject *Sender);
    void __fastcall StopExit(TObject *Sender);
    void __fastcall CreateTreeClick(TObject *Sender);
    void __fastcall ShowIndClick(TObject *Sender);
    void __fastcall BadBirthClick(TObject *Sender);
private:	// User declarations
    TStringList *GEDFile;
    TStringList *Individuals;
    TStringList *Families;
    TStringList *Months;
    int NumIndividuals;
    int NumFamilies;
    int CurrYear;
    bool SameLastMarriage(Individual *Indi, Individual **Spouse);
    void __fastcall AddName(Individual *Indi);
    void __fastcall AddAnn(Family *Fam);
public:		// User declarations
    void __fastcall MakeAnImage(AnsiString InputName,
                                int AllowedWidth,
                                int AllowedHeight,
                                AnsiString OutputName,
                                AnsiString &WidthNHeight);
    void __fastcall IndDetails(Individual *Ind,
                               TStringList **HTML,
                               int fam);
    void __fastcall PrintFsM(Individual *Indi);
    __fastcall TMainForm(TComponent* Owner);
    void __fastcall ParseRelations();
    __fastcall ~TMainForm();
    bool CancelTree;
};
//---------------------------------------------------------------------------
extern PACKAGE TMainForm *MainForm;
//---------------------------------------------------------------------------
#endif
