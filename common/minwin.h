#ifndef minimum_windows_declarations_h
#define minimum_windows_declarations_h

//Tell windows.h not to make declarations that we do not use
//See windows.h for descriptions of what is turned off by what.

#define NOGDICAPMASKS     
#define NOVIRTUALKEYCODES 
#define NOWINMESSAGES     
#define NOWINSTYLES       
#define NOSYSMETRICS      
#define NOMENUS           
#define NOICONS           
#define NOKEYSTATES       
#define NOSYSCOMMANDS     
#define NORASTEROPS       
#define NOSHOWWINDOW      
#define OEMRESOURCE       
#define NOATOM            
#define NOCLIPBOARD       
#define NOCOLOR           
#define NOCTLMGR          
#define NODRAWTEXT        
#define NOGDI             
#define NOKERNEL          
#define NOUSER            
#define NONLS             
#define NOMB              
#define NOMEMMGR          
#define NOMETAFILE        
#define NOMINMAX          
#define NOMSG             
#define NOOPENFILE        
#define NOSCROLL          
#define NOSERVICE         
#define NOSOUND           
#define NOTEXTMETRIC      
#define NOWH              
#define NOWINOFFSETS      
#define NOCOMM            
#define NOKANJI           
#define NOHELP            
#define NOPROFILER        
#define NODEFERWINDOWPOS  
#define NOCRYPT
#define NOMCX             

# include <windows.h>
#endif
