; get file
FileSelectFile, srcFile, 3, ,Get Payload, Text Documents (*.txt; *.doc )
if ( srcFile == "" ) 
{
    exitapp
    return
}
MsgBox "opened [ " + %srcFile% + "]"

; read file
; FileRead, text, %file%

; exit key
esc::
{
    MsgBox "good bye"
    exitapp
}

+s::
{
   loop
   {
        loop, read, %srcFile%
        {
          send %A_LoopReadLine%
          ;  loop, parse, A_LoopReadLine, `n, `r 
          ; {
          ;    send, %A_LoopField%   
          ;    sleep 500
          ; }
          send, {enter}
          sleep 2000
        }    
        sleep 12
   }
}

; daemon
;Loop
;{
;        Sleep 18
;
;}