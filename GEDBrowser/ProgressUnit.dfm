object Progress: TProgress
  Left = 196
  Top = 351
  BorderIcons = [biSystemMenu]
  BorderStyle = bsDialog
  Caption = 'Tree creation progress...'
  ClientHeight = 99
  ClientWidth = 500
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  OldCreateOrder = False
  Position = poScreenCenter
  PixelsPerInch = 96
  TextHeight = 13
  object Bar: TProgressBar
    Left = 80
    Top = 24
    Width = 345
    Height = 25
    Min = 0
    Max = 100
    TabOrder = 0
  end
  object CancelBtn: TButton
    Left = 200
    Top = 64
    Width = 75
    Height = 25
    Cancel = True
    Caption = '&Cancel'
    Default = True
    TabOrder = 1
    OnClick = CancelBtnClick
  end
end
