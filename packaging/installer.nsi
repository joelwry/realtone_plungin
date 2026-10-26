; RealTone Studio installer (NSIS 3, Unicode)
;
;   makensis /DVERSION=0.1.0 /DSTAGE=C:\path\to\stage /DOUTFILE=RealToneStudio-0.1.0-win64-setup.exe packaging\installer.nsi
;
; STAGE layout:
;   app\      RealTone Studio.exe, realtone-cli.exe, README.txt, CHANGELOG.md, THIRD_PARTY_NOTICES.txt, docs\..., examples\...
;   vst3\     RealTone Studio.vst3\Contents\...   (the complete bundle folder)
;
; The installer writes only inside its own program folder and inside "RealTone Studio.vst3" in the
; common VST3 folder. The uninstaller removes exactly what was installed. Projects, presets, logs,
; settings and downloaded sample libraries live under %LOCALAPPDATA%\RealToneStudio and are never touched.

Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef STAGE
  !error "Pass /DSTAGE=<staging folder>"
!endif
!ifndef OUTFILE
  !define OUTFILE "RealToneStudio-${VERSION}-win64-setup.exe"
!endif

!define APPNAME "RealTone Studio"
!define PUBLISHER "RealTone"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\RealToneStudio"
!define VST3BUNDLE "RealTone Studio.vst3"

Name "${APPNAME}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${APPNAME}"
InstallDirRegKey HKLM "Software\RealToneStudio" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "${APPNAME} ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${APPNAME}"
VIAddVersionKey "FileDescription" "${APPNAME} installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"
VIAddVersionKey "CompanyName" "${PUBLISHER}"
VIAddVersionKey "LegalCopyright" "See THIRD_PARTY_NOTICES.txt"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APPNAME}.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${APPNAME}"
!define MUI_FINISHPAGE_NOREBOOTSUPPORT

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\app\THIRD_PARTY_NOTICES.txt"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "${APPNAME} needs 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section "${APPNAME} application" SecApp
  SectionIn RO
  SetRegView 64
  SetOutPath "$INSTDIR"
  File "${STAGE}\app\RealTone Studio.exe"
  File "${STAGE}\app\realtone-cli.exe"
  File "${STAGE}\app\README.txt"
  File "${STAGE}\app\CHANGELOG.md"
  File "${STAGE}\app\THIRD_PARTY_NOTICES.txt"
  SetOutPath "$INSTDIR\docs"
  File "${STAGE}\app\docs\*.md"
  SetOutPath "$INSTDIR\examples"
  File "${STAGE}\app\examples\demo_phrase.mid"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\RealToneStudio" "InstallDir" "$INSTDIR"

  CreateDirectory "$SMPROGRAMS\${APPNAME}"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk" "$INSTDIR\${APPNAME}.exe"
  CreateShortcut "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "${APPNAME}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "${PUBLISHER}"
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\${APPNAME}.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" "$0"
SectionEnd

Section "VST3 instrument (for FL Studio, REAPER and other DAWs)" SecVst3
  SetRegView 64
  ; Only this plug-in's own folder inside the shared VST3 directory is written.
  SetOutPath "$COMMONFILES64\VST3\${VST3BUNDLE}\Contents\x86_64-win"
  File "${STAGE}\vst3\${VST3BUNDLE}\Contents\x86_64-win\RealTone Studio.vst3"
  SetOutPath "$COMMONFILES64\VST3\${VST3BUNDLE}\Contents\Resources"
  File "${STAGE}\vst3\${VST3BUNDLE}\Contents\Resources\moduleinfo.json"
  WriteRegStr HKLM "Software\RealToneStudio" "Vst3Installed" "1"
SectionEnd

LangString DESC_App ${LANG_ENGLISH} "The standalone application (MIDI and audio conversion, WAV export), command line tool and documentation."
LangString DESC_Vst3 ${LANG_ENGLISH} "The VST3 instrument, installed to C:\Program Files\Common Files\VST3. Audacity cannot host instrument plug-ins; use the standalone application to render audio for Audacity."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecApp} $(DESC_App)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVst3} $(DESC_Vst3)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  SetRegView 64

  ; Program files: exactly the ones installed above.
  Delete "$INSTDIR\RealTone Studio.exe"
  Delete "$INSTDIR\realtone-cli.exe"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\CHANGELOG.md"
  Delete "$INSTDIR\THIRD_PARTY_NOTICES.txt"
  Delete "$INSTDIR\docs\*.md"
  Delete "$INSTDIR\examples\demo_phrase.mid"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR\docs"
  RMDir "$INSTDIR\examples"
  RMDir "$INSTDIR"             ; fails (and leaves the folder) if the user added files of their own

  Delete "$SMPROGRAMS\${APPNAME}\${APPNAME}.lnk"
  Delete "$SMPROGRAMS\${APPNAME}\Uninstall ${APPNAME}.lnk"
  RMDir "$SMPROGRAMS\${APPNAME}"

  ; VST3: our own files, then the empty folders. /REBOOTOK covers a DAW that still has the DLL loaded.
  Delete /REBOOTOK "$COMMONFILES64\VST3\${VST3BUNDLE}\Contents\x86_64-win\RealTone Studio.vst3"
  Delete "$COMMONFILES64\VST3\${VST3BUNDLE}\Contents\Resources\moduleinfo.json"
  RMDir "$COMMONFILES64\VST3\${VST3BUNDLE}\Contents\x86_64-win"
  RMDir "$COMMONFILES64\VST3\${VST3BUNDLE}\Contents\Resources"
  RMDir "$COMMONFILES64\VST3\${VST3BUNDLE}\Contents"
  RMDir "$COMMONFILES64\VST3\${VST3BUNDLE}"

  DeleteRegKey HKLM "${UNINSTKEY}"
  DeleteRegKey HKLM "Software\RealToneStudio"
  ; %LOCALAPPDATA%\RealToneStudio (projects, presets, settings, logs, sample libraries) is deliberately left alone.
SectionEnd
