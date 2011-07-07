/*
 The Aspell library can be built as a dll or a static library.
 The dll needs to save and retieve its own instance handle.
 The static library does not have an instance handle, so it uses 0
 which is the main executables handle.
*/

#ifdef WIN32PORT
void * get_module_handle()
{
	return 0;
}
#endif

