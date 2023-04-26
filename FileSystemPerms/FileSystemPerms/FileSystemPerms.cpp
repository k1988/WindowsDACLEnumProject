/*
A Microsoft Windows file system permissions dumper with suspicious DACL alerting

Released as open source by NCC Group Plc - http://www.nccgroup.com/

Developed by Ollie Whitehouse, ollie dot whitehouse at nccgroup dot com

https://github.com/nccgroup/WindowsDACLEnumProject

Released under AGPL see LICENSE.txt for more information
*/


#include "stdafx.h"
#include "XGetopt.h"
#include "sddl.h"
#include <initializer_list>
#include <map>
#include <string>

#define MAGIC (0xa0000000L)

//
// Globals
//
bool	bExclude=false;

// �Ƿ��ӡsd���ַ�����ʽ
bool bSDStr = false;

//
//
//
//
bool UsersWeCareAbout(char *lpDomain, char *lpName)
{
	
	if(strcmp(lpDomain,"NT AUTHORITY") == 0 && strcmp(lpName,"SYSTEM") ==0 ) return false;
	if(strcmp(lpDomain,"NT AUTHORITY") == 0 && strcmp(lpName,"NETWORK SERVICE") ==0 ) return false;
	if(strcmp(lpDomain,"NT AUTHORITY") == 0 && strcmp(lpName,"LOCAL SERVICE") ==0 ) return false;
	else if(strcmp(lpDomain,"BUILTIN") == 0 && strcmp(lpName,"Users") ==0) return true;
	else if(strcmp(lpDomain,"BUILTIN") == 0) return false;
	else if(strcmp(lpDomain,"NT SERVICE") == 0) return false;
	else if(strcmp(lpDomain,"NT AUTHORITY") == 0 && strcmp(lpName,"SERVICE") == 0) return false;
	else if(strcmp(lpDomain,"NT AUTHORITY") == 0 && strcmp(lpName,"INTERACTIVE") == 0) return false;
	else {
		//fprintf(stdout,"- %s we care",lpName);
		return true;
	}
}


//
// Function	: sidToText
// Role		: Converts a binary SID to a nice one
// Notes	: http://win32.mvps.org/security/dumpacl/dumpacl.cpp
//
const char *sidToText( PSID psid )
{
	// S-rev- + SIA + subauthlen*maxsubauth + terminator
	static char buf[15 + 12 + 12*SID_MAX_SUB_AUTHORITIES + 1];
	char *p = &buf[0];
	PSID_IDENTIFIER_AUTHORITY psia;
	DWORD numSubAuths, i;

	// Validate the binary SID.

	if ( ! IsValidSid( psid ) )
		return FALSE;

	psia = GetSidIdentifierAuthority( psid );

	p = buf;
	p += _snprintf_s( p, 15 + 12 + 12*SID_MAX_SUB_AUTHORITIES + 1, &buf[sizeof buf] - p, "S-%lu-", 0x0f & *( (byte *) psid ) );

	if ( ( psia->Value[0] != 0 ) || ( psia->Value[1] != 0 ) )
		p += _snprintf_s( p,15 + 12 + 12*SID_MAX_SUB_AUTHORITIES + 1, &buf[sizeof buf] - p, "0x%02hx%02hx%02hx%02hx%02hx%02hx",
			(USHORT) psia->Value[0], (USHORT) psia->Value[1],
			(USHORT) psia->Value[2], (USHORT) psia->Value[3],
			(USHORT) psia->Value[4], (USHORT) psia->Value[5] );
	else
		p += _snprintf_s( p, 15 + 12 + 12*SID_MAX_SUB_AUTHORITIES + 1, &buf[sizeof buf] - p, "%lu", (ULONG) ( psia->Value[5] ) +
			(ULONG) ( psia->Value[4] << 8 ) + (ULONG) ( psia->Value[3] << 16 ) +
			(ULONG) ( psia->Value[2] << 24 ) );

	// Add SID subauthorities to the string.

	numSubAuths = *GetSidSubAuthorityCount( psid );
	for ( i = 0; i < numSubAuths; ++ i )
		p += _snprintf_s( p, 15 + 12 + 12*SID_MAX_SUB_AUTHORITIES + 1,&buf[sizeof buf] - p, "-%lu", *GetSidSubAuthority( psid, i ) );

	return buf;
}

//
//
//
//
void PrintPermissions(PACL DACL, bool bFile)
{

	DWORD					dwRet=0;
	DWORD					dwCount=0;
	ACCESS_ALLOWED_ACE		*ACE;
	
	// http://msdn2.microsoft.com/en-us/library/aa379142.aspx
	if(IsValidAcl(DACL) == TRUE){

		// Now for each ACE in the DACL
		for(dwCount=0;dwCount<DACL->AceCount;dwCount++){
			// http://msdn2.microsoft.com/en-us/library/aa446634.aspx
			// http://msdn2.microsoft.com/en-us/library/aa379608.aspx
			if(GetAce(DACL,dwCount,(LPVOID*)&ACE)){
				// http://msdn2.microsoft.com/en-us/library/aa374892.aspx		
				SID *sSID = (SID*)&(ACE->SidStart);
				if(sSID != NULL)
				{
					DWORD dwSize = 2048;
					char lpName[2048];
					char lpDomain[2048];
					SID_NAME_USE SNU;
					
					switch(ACE->Header.AceType){
						// Allowed ACE
						case ACCESS_ALLOWED_ACE_TYPE:
							// Lookup the account name and print it.										
							// http://msdn2.microsoft.com/en-us/library/aa379554.aspx
							if( !LookupAccountSidA( NULL, sSID, lpName, &dwSize, lpDomain, &dwSize, &SNU ) ) {
								
								DWORD dwResult = GetLastError();
								if(dwResult == ERROR_NONE_MAPPED && bExclude == true){
									break;
								} else if( dwResult == ERROR_NONE_MAPPED && bExclude == false){
									fprintf(stdout,"[i]   |\n");
									fprintf(stdout,"[i]   +-+-> Allowed 2 - NONMAPPED - SID %s\n", sidToText(sSID));
								} else if (dwResult != ERROR_NONE_MAPPED){
									fprintf(stderr,"[!] LookupAccountSid Error 	%u\n", GetLastError());
									fprintf(stdout,"[i]   |\n");
									fprintf(stdout,"[i]   +-+-> Allowed - ERROR     - SID %s\n", sidToText(sSID));
									//return;
								} else {
									continue;
								}
							} else {
								
								fprintf(stdout,"[i]     |\n");
								fprintf(stdout,"[i]     +-+-> Allowed - %s\\%s\n",lpDomain,lpName);
							}
							
							// print out the ACE mask
							fprintf(stdout,"[i]       |\n");
							fprintf(stdout,"[i]       +-> Permissions - ");
							
						
							if(bFile == false){
								if(ACE->Mask & FILE_GENERIC_EXECUTE) fprintf(stdout,",Generic Execute");
								if(ACE->Mask & FILE_GENERIC_READ   ) fprintf(stdout,",Generic Read");
								if(ACE->Mask & FILE_GENERIC_WRITE   ) fprintf(stdout,",Generic Write");
								if(ACE->Mask & GENERIC_ALL) fprintf(stdout,",Generic All");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_DELETE_CHILD)) fprintf(stdout,",Delete diretory and files - Alert");
								else if(ACE->Mask & FILE_DELETE_CHILD) fprintf(stdout,",Delete diretory and files");

								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_ADD_FILE)) fprintf(stdout,",Add File - Alert");
								else if(ACE->Mask & FILE_ADD_FILE) fprintf(stdout,",Add File");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_WRITE_EA)) fprintf(stdout,",Write Extended Attributes - Alert");
								else if(ACE->Mask & FILE_WRITE_EA) fprintf(stdout,",Write Extended Attributes");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_WRITE_ATTRIBUTES)) fprintf(stdout,",Write Attributes - Alert");
								else if(ACE->Mask & FILE_WRITE_ATTRIBUTES) fprintf(stdout,",Write Attributes");

								if(ACE->Mask & FILE_READ_EA) fprintf(stdout,",Read Extended Attributes");

								if(ACE->Mask & FILE_READ_ATTRIBUTES) fprintf(stdout,",Read Attributes");
								
								if(ACE->Mask & FILE_LIST_DIRECTORY) fprintf(stdout,",List Directory");
								if(ACE->Mask & FILE_READ_EA) fprintf(stdout,",Read Extended Attributes");
								if(ACE->Mask & FILE_ADD_SUBDIRECTORY) fprintf(stdout,",Add Subdirectory");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_TRAVERSE)) fprintf(stdout,",Traverse Directory - Alert");
								else if (ACE->Mask & FILE_TRAVERSE) fprintf(stdout,",Traverse Directory");

								if(ACE->Mask & STANDARD_RIGHTS_READ) fprintf(stdout,",Read DACL");
								if(ACE->Mask & STANDARD_RIGHTS_WRITE) fprintf(stdout,",Write DACL");
								

								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & WRITE_DAC)) fprintf(stdout,",Change Permissions - Alert");
								else if(ACE->Mask & WRITE_DAC) fprintf(stdout,",Change Permissions");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & WRITE_OWNER)) fprintf(stdout,",Change Owner - Alert");
								else if(ACE->Mask & READ_CONTROL) fprintf(stdout,",Change Owner");

								if(ACE->Mask & READ_CONTROL) fprintf(stdout,",Read Control");
								if(ACE->Mask & DELETE) fprintf(stdout,",Delete");
								if(ACE->Mask & SYNCHRONIZE) fprintf(stdout,",Synchronize");

								// http://www.grimes.nildram.co.uk/workshops/secWSNine.htm
								if(ACE->Mask & MAGIC) fprintf(stdout,",Generic Read OR Generic Write");
							} 
							else 
							{
								if(ACE->Mask & FILE_GENERIC_EXECUTE) fprintf(stdout,",Generic Execute");
								if(ACE->Mask & FILE_GENERIC_READ   ) fprintf(stdout,",Generic Read");
								if(ACE->Mask & FILE_GENERIC_WRITE   ) fprintf(stdout,",Generic Write");
								if(ACE->Mask & GENERIC_ALL) fprintf(stdout,",Generic All");

								if(ACE->Mask & FILE_GENERIC_EXECUTE) fprintf(stdout,",Execute");
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_WRITE_ATTRIBUTES)) fprintf(stdout,",Write Attributes - Alert");
								else if(ACE->Mask & FILE_WRITE_ATTRIBUTES) fprintf(stdout,",Write Attributes");

								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_WRITE_DATA)) fprintf(stdout,",Write Data - Alert");
								else if(ACE->Mask & FILE_WRITE_DATA) fprintf(stdout,",Write Data");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & FILE_WRITE_EA)) fprintf(stdout,",Write Extended Attributes - Alert");
								else if(ACE->Mask & FILE_WRITE_EA) fprintf(stdout,",Write Extended Attributes");

								if(ACE->Mask & FILE_READ_ATTRIBUTES) fprintf(stdout,",Read Attributes");
								if(ACE->Mask & FILE_READ_DATA) fprintf(stdout,",Read Data");
								if(ACE->Mask & FILE_READ_EA) fprintf(stdout,",Read Extended Attributes");
								if(ACE->Mask & FILE_APPEND_DATA) fprintf(stdout,",Append");
								if(ACE->Mask & FILE_EXECUTE) fprintf(stdout,",Execute");

								if(ACE->Mask & STANDARD_RIGHTS_READ) fprintf(stdout,",Read DACL");
								if(ACE->Mask & STANDARD_RIGHTS_WRITE) fprintf(stdout,",Read DACL");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & WRITE_DAC)) fprintf(stdout,",Change Permissions - Alert");
								else if(ACE->Mask & WRITE_DAC) fprintf(stdout,",Change Permissions");
								
								if(UsersWeCareAbout(lpDomain,lpName) == true && (ACE->Mask & WRITE_OWNER)) fprintf(stdout,",Change Owner - Alert");
								else if(ACE->Mask & WRITE_OWNER) fprintf(stdout,",Change Owner");

								if(ACE->Mask & READ_CONTROL) fprintf(stdout,",Read Control");
								if(ACE->Mask & DELETE) fprintf(stdout,",Delete");
								if(ACE->Mask & SYNCHRONIZE) fprintf(stdout,",Synchronize");

								// http://www.grimes.nildram.co.uk/workshops/secWSNine.htm
								if(ACE->Mask & MAGIC) fprintf(stdout,",Generic Read OR Generic Write");

							}
							fprintf(stdout,"\n");
							break;
						// Denied ACE
						case ACCESS_DENIED_ACE_TYPE:
							break;
						// Uh oh
						default:
							break;
					}

					
				}
			} else {
				DWORD dwError = GetLastError();
				fprintf(stderr,"[!] Error - %d - GetAce\n", dwError);
				return;
			}
		}
	} else {
		DWORD dwError = GetLastError();
		fprintf(stderr,"[!] Error - %d - IsValidAcl\n", dwError);
		return;
	}


}

// ��ӡ�ļ����������͵�SecurityDescriptor
void PrintAllSecurityDescriptor(char* strFile)
{
	DWORD dwSize =0;
	DWORD dwBytesNeeded =0;	

    const std::initializer_list<SECURITY_INFORMATION> informationList =
    {
             OWNER_SECURITY_INFORMATION,
             GROUP_SECURITY_INFORMATION,
             DACL_SECURITY_INFORMATION,
             SACL_SECURITY_INFORMATION,
             LABEL_SECURITY_INFORMATION,
             ATTRIBUTE_SECURITY_INFORMATION,
             SCOPE_SECURITY_INFORMATION,
             PROCESS_TRUST_LABEL_SECURITY_INFORMATION,
             ACCESS_FILTER_SECURITY_INFORMATION,
             BACKUP_SECURITY_INFORMATION,
             PROTECTED_DACL_SECURITY_INFORMATION,
             PROTECTED_SACL_SECURITY_INFORMATION,
             UNPROTECTED_DACL_SECURITY_INFORMATION,
             UNPROTECTED_SACL_SECURITY_INFORMATION
    };
	
	static std::map<SECURITY_INFORMATION, const char*> security_information_map = {  
    {OWNER_SECURITY_INFORMATION, "Owner Security Information"},  
    {GROUP_SECURITY_INFORMATION, "Group Security Information"},  
    {DACL_SECURITY_INFORMATION, "DACL Security Information"},  
    {SACL_SECURITY_INFORMATION, "SACL Security Information"},  
    {LABEL_SECURITY_INFORMATION, "Label Security Information"},  
    {ATTRIBUTE_SECURITY_INFORMATION, "Attribute Security Information"},  
    {SCOPE_SECURITY_INFORMATION, "Scope Security Information"},  
    {PROCESS_TRUST_LABEL_SECURITY_INFORMATION, "Process Trust Label Security Information"},  
    {ACCESS_FILTER_SECURITY_INFORMATION, "Access Filter Security Information"},  
    {BACKUP_SECURITY_INFORMATION, "Backup Security Information"},  
    {PROTECTED_DACL_SECURITY_INFORMATION, "Protected DACL Security Information"},  
    {PROTECTED_SACL_SECURITY_INFORMATION, "Protected SACL Security Information"},  
    {UNPROTECTED_DACL_SECURITY_INFORMATION, "Unspecified DACL Security Information"},  
    {UNPROTECTED_SACL_SECURITY_INFORMATION, "Unspecified SACL Security Information"},  
    };

    for (const SECURITY_INFORMATION information : informationList)
    {
        const auto informationName = security_information_map[information];

        GetFileSecurity(strFile, information, NULL, NULL, &dwBytesNeeded);
        dwSize = dwBytesNeeded;
        PSECURITY_DESCRIPTOR* secDesc = (PSECURITY_DESCRIPTOR*)LocalAlloc(LMEM_FIXED, dwBytesNeeded);
        if (GetFileSecurity(strFile, information, secDesc, dwSize, &dwBytesNeeded) == false) 
		{
			 fprintf(stdout, "!! %x %s -> Failed %d\n", information, informationName, GetLastError());
            continue;
        }

		// convert to string and print
        LPTSTR pStringSecurityDescriptor = nullptr;
        ULONG pStringSecurityDescriptorSize = 0;
        ConvertSecurityDescriptorToStringSecurityDescriptor(secDesc, SDDL_REVISION_1, information, &pStringSecurityDescriptor, &pStringSecurityDescriptorSize);
        if (pStringSecurityDescriptorSize > 1)
        {
            fprintf(stdout, "%x %s -> %s\n", information, informationName, pStringSecurityDescriptor);
        }

        LocalFree(pStringSecurityDescriptor);
    }

}

// ��ȡ�ļ���SD������ΪDACL_SECURITY_INFORMATION����ʹ��PrintPermissions��ӡ���е����Ȩ��
bool GetHandleBeforePrint(char* strFile){
	DWORD dwSize =0;
	DWORD dwBytesNeeded =0;
	
	GetFileSecurity (strFile,DACL_SECURITY_INFORMATION,NULL,NULL,&dwBytesNeeded);
	dwSize = dwBytesNeeded;
	PSECURITY_DESCRIPTOR* secDesc = (PSECURITY_DESCRIPTOR*)LocalAlloc(LMEM_FIXED,dwBytesNeeded);
	if(GetFileSecurity (strFile,DACL_SECURITY_INFORMATION,secDesc,dwSize,&dwBytesNeeded) == false){
		fprintf(stdout,"[i] |\n");
		fprintf(stdout,"[i] +-+-> Failed to query file system object security - %d\n",GetLastError());
		return false;
    }
	
	PACL DACL;
	BOOL bDACLPresent = false;
	BOOL bDACLDefaulted = false;


	bDACLPresent = false;
	bDACLDefaulted = false;
	if(GetSecurityDescriptorDacl(secDesc,&bDACLPresent,&DACL,&bDACLDefaulted) == false){
		fprintf(stdout,"[i] |\n");
		fprintf(stdout,"[i] +-+-> Failed to get security descriptor - %d\n",GetLastError());
		return false;
	}

	PrintPermissions(DACL,true);

	return true;
}

bool ListFiles(char *strPath) {
    HANDLE hFind = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATA ffdThis;
         
    
	char strThisSpec[MAX_PATH];
	sprintf_s(strThisSpec,MAX_PATH,"%s\\*.*",strPath);

	hFind = FindFirstFile(strThisSpec, &ffdThis);
	if (hFind == INVALID_HANDLE_VALUE)  {
		return false;
	} 

	do {
		if (strcmp(ffdThis.cFileName, ".") != 0 && 
			strcmp(ffdThis.cFileName, "..") != 0) {
            if (ffdThis.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				char strFoo[MAX_PATH];
				sprintf_s(strFoo,MAX_PATH,"%s\\%s",strPath,ffdThis.cFileName);
				fprintf(stdout,"[directory] %s \n",strFoo);
				GetHandleBeforePrint(strFoo);				
				if (bSDStr) PrintAllSecurityDescriptor(strFoo);
				ListFiles(strFoo);
            }
			else 
			{
                char strFoo[MAX_PATH];
				sprintf_s(strFoo,MAX_PATH,"%s\\%s",strPath,ffdThis.cFileName);
				fprintf(stdout,"[file] %s \n",strFoo);
				GetHandleBeforePrint(strFoo);
				if (bSDStr) PrintAllSecurityDescriptor(strFoo);
			}
		}
	} while (FindNextFile(hFind, &ffdThis) != 0);

	if (GetLastError() != ERROR_NO_MORE_FILES) {
		FindClose(hFind);
		return false;
	}

	FindClose(hFind);
	hFind = INVALID_HANDLE_VALUE;


    return true;
}


//
// Function	: PrintHelp
// Role		: 
// Notes	: 
// 
void PrintHelp(char *strExe){

	fprintf (stdout,"    i.e. %s [-p] [-x] [-h]\n",strExe);
	fprintf (stdout,"    -p [PATH] Path to use instead of C:\\\n");	
	fprintf (stdout,"    -x exclude non mapped SIDs from alerts\n");
	fprintf (stdout,"    -s print file's all type SD in string format\n");
	fprintf (stdout,"\n");
	ExitProcess(1);
}


int EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

int _tmain(int argc, _TCHAR* argv[])
{

	bool	bHelp=false;
	TCHAR	*strPath=NULL;
	char	chOpt;

	printf("[*] Windows DACL Enumeration Project - https://github.com/nccgroup/WindowsDACLEnumProject - FileSystemPerms\n");
	printf("[*] NCC Group Plc - http://www.nccgroup.com/ \n");
	printf("[*] -h for help \n");

	// Extract all the options
	while ((chOpt = getopt(argc, argv, _T("p:hxs"))) != EOF) 
	switch(chOpt)
	{
		case _T('p'):
			strPath=optarg;
			break;
		case _T('x'):
			bExclude=true;
			break;
		case _T('h'): // Help
			bHelp=true;
			break;
		case _T('s'): 
			bSDStr=true;
			break;
		default:
			fwprintf(stderr,L"[!] No handler - %c\n", chOpt);
			break;
	}

	if(bHelp) PrintHelp(argv[0]);

	if(strPath) {

		while(EndsWith(strPath," ") == true){
			char *strPtr = strrchr(strPath,' ');
			*strPtr = 0x00;
		}

		if(EndsWith(strPath,"\"") == true){
			char *strPtr = strrchr(strPath,'\"');
			*strPtr = 0x00;
		} 
		fprintf(stdout,"[i] Path now %s \n", strPath);
		ListFiles(strPath);
	}
	else ListFiles("C:\\");
        
    

	return 0;
}

