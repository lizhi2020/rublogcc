#include<iostream>
#include<vector>
#include<string>
#include<sstream>
#include<Windows.h>

namespace rub {
using namespace std;
// ��һ��״̬��������md�ļ�
class TM {
	const string& content;
	string html;
public:
	TM(const string& content):content(content) {};
	// ����ֵ��ʾ�Ƿ���Լ�������һ��
	bool processLine() {

	};
	string& process() {
		html.clear();
		// while (processLine()) {};
		html = string(content);
		return html;
	}
};
string md2html(const string& content) {
	TM tm = TM(content);
	return tm.process();
}
} // rub


void genIndex(LPCWSTR dir, const std::vector<std::wstring>& items) {
	// ����index.html�ļ�
	TCHAR dst[100]=L"public";
	lstrcat(dst, dir+sizeof(TEXT("content"))/sizeof(TCHAR)-1);
	lstrcat(dst, TEXT("\\index.html"));

	char buffer[100];

	// д���ļ�
	HANDLE file = CreateFile(dst, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
		return;

	std::stringstream stream;
	stream << "<html><head><meta charset='utf-8'></head><body>";
	for (auto& i : items) {
		char buffer[100];
		memset(buffer, 0, 100);
		WideCharToMultiByte(CP_UTF8, 0, i.c_str(), i.size(), buffer, 100, NULL, NULL);
		std::string href = std::string(buffer);
		stream << "<a href=\"" << href << "\">" << href << "</a><br/>";// ʹ��br����
	}
	stream << "</body></html>";
	auto r = stream.str();
	WriteFile(file, r.c_str(), r.size(), nullptr, nullptr);
	CloseHandle(file);
}

// �ݹ����
void walk(LPCWSTR path) {
	WIN32_FIND_DATA fdata;
	HANDLE hFind;
	TCHAR buf[100];

	std::vector<std::wstring> items;

	lstrcpy(buf, path);
	lstrcat(buf, TEXT("\\*"));

	hFind = FindFirstFile(buf, &fdata);
	if (hFind == INVALID_HANDLE_VALUE) {
		return;
	}
	do {
		if (lstrcmp(fdata.cFileName,TEXT("."))==0||
			lstrcmp(fdata.cFileName,TEXT(".."))==0) {
			continue;
		}
		
		if (fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			lstrcpy(buf, path);
			lstrcat(buf, TEXT("\\"));
			lstrcat(buf, fdata.cFileName);
			TCHAR dst[100]=TEXT("public");
			lstrcat(dst, buf + sizeof(TEXT("content")) / sizeof(TCHAR) - 1);
			CreateDirectory(dst, NULL);
			walk(buf);
			items.push_back(fdata.cFileName);
			continue;
		}
		// ����Ƿ���md�ļ�
		int length = lstrlen(fdata.cFileName);
		if (length < 3) {
			continue;
		}
		if (lstrcmp(TEXT(".md"),fdata.cFileName+length-3)!=0) {
			continue;
		}
		items.push_back(fdata.cFileName);
		TCHAR src[100];
		TCHAR dst[100]=TEXT("public");
		lstrcpy(src, path);
		lstrcat(src, TEXT("\\"));
		lstrcat(src, fdata.cFileName);
		lstrcat(dst, src + sizeof(TEXT("content")) / sizeof(TCHAR)-1);

		CopyFile(src, dst, false);
	} while (FindNextFile(hFind, &fdata));

	genIndex(path, items);
}
int main() {
	// ��������
	std::locale::global(std::locale(""));

	// ����contentĿ¼ ��publicĿ¼�������Ӧ�ļ�
	walk(TEXT("content"));
	// genIndex(TEXT("content"), {L"dairy",L"public"});
}