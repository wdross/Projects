//---------------------------------------------------------------------------

#ifndef ProgressUnitH
#define ProgressUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
//---------------------------------------------------------------------------
class TProgress : public TForm
{
__published:	// IDE-managed Components
        TProgressBar *Bar;
        TButton *CancelBtn;
        void __fastcall CancelBtnClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
        __fastcall TProgress(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TProgress *Progress;
//---------------------------------------------------------------------------
#endif
