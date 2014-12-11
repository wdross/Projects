//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
USERES("GEDBrowser.res");
USEFORM("Main.cpp", MainForm);
USEFORM("ProgressUnit.cpp", Progress);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
        try
        {
                 Application->Initialize();
                 Application->CreateForm(__classid(TMainForm), &MainForm);
                 Application->CreateForm(__classid(TProgress), &Progress);
                 Application->Run();
        }
        catch (Exception &exception)
        {
                 Application->ShowException(&exception);
        }
        return 0;
}
//---------------------------------------------------------------------------
