// Abstract.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include "segment_dll.h"
#include "postag_dll.h"
#include "parser_dll.h"
using namespace std;

int GBK2UTF8(const char *szGbk, char *szUtf8);
int UTF82GBK(const char *szUtf8, char *szGbk);
int GetGroupId(vector <int>& group, const int wid, int& groupNum);
int MergeGroup(vector <int>& group, const int fromId, const int toId, int& headGroup);
int FindNextSubSentenceStart(string& line, const int startPos);

int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		cout << "Abstract.exe <ModelPath> <InputFile> <OutputFile>" << endl;
		return -1;
	}
	
	char segModel[256] = {0};
	char posModel[256] = {0};
	char psrModel[256] = {0};
	sprintf_s(segModel, "%s\\cws.model", argv[1]);
	sprintf_s(posModel, "%s\\pos.model", argv[1]);
	sprintf_s(psrModel, "%s\\parser.model", argv[1]);

	void* segEngine = segmentor_create_segmentor(segModel);
	if(NULL == segEngine)
	{
		cout << "Failed to initialize segmentor engine" << endl;
		return -2;
	}

	void* posEngine = postagger_create_postagger(posModel);
	if(NULL == posEngine)
	{
		cout << "Failed to initialize postagger engine" << endl;
		return -3;
	}

	void* parseEngine = parser_create_parser(psrModel);
	if(NULL == parseEngine)
	{
		cout << "Failed to initialize parser engine" << endl;
		return -4;
	}

	vector<string> words;
	vector<string> tags;
	vector<int>    heads;
	vector<string> deprels;
	vector<int>    group;

	ifstream fin;
	ofstream fout;
	const int maxLineLen = 1024;
	char str[maxLineLen] = {0};
	string line = "";

	fin.open(argv[2], ios::in);
	if(!fin)
	{
		cout << "open input file failed" << endl;
		return -5;
	}

	while(getline(fin, line))
	{
		if(line.length() > 0)
		{
			int startPos = 0;
			bool bExit = false;
			string result = "";
			while(!bExit)
			{
				int pos = FindNextSubSentenceStart(line, startPos);
				string sentence = "";
				if(-1 == pos)
				{
					sentence = line.substr(startPos);
					bExit = true;
				}
				else
				{
					sentence = line.substr(startPos, pos - startPos);
					startPos = pos;
				}

				// xxxx的原因:
				if(-1 != sentence.find("原因") && (-1 != sentence.find("：") || -1 != sentence.find(":"))) continue;

				words.clear();
				tags.clear();
				heads.clear();
				deprels.clear();
				GBK2UTF8(sentence.c_str(), str);
				const int len = segmentor_segment(segEngine, str, words);
				postagger_postag(posEngine, words, tags);
				parser_parse(parseEngine, words, tags, heads, deprels);

				for(size_t i = 0; i < heads.size(); ++i)
				{
					UTF82GBK(words[i].c_str(), str);
					std::cout << (i+1) << "\t" << str << "\t" << tags[i] << "\t" 
						<< heads[i] << "\t" << deprels[i] << std::endl;
				}

				int groupNum = 0;
				int headGroup = 0;
				group.clear();
				for(int i = 0; i < len; ++i)
				{
					group.push_back(0);
				}

				for(int i = 0; i < len; ++i)
				{
					if("HED" == deprels[i])
					{
						headGroup = GetGroupId(group, i, groupNum);
					}
					else if(deprels[i] == "VOB" || deprels[i] == "WP" || deprels[i] == "COO" || deprels[i] == "SBV" ||
						deprels[i] == "LAD" || deprels[i] == "RAD" || deprels[i] == "FOB" || deprels[i] == "POB" ||
						(deprels[i] == "ATT" && (tags[i] == "v" || tags[i] == "m" || tags[i] == "n")) ||
						(deprels[i] == "ADV" && (tags[i] == "c" || tags[i] == "m" || tags[i] == "a" || tags[i] == "p")))
					{
						if(group[i] > 0)
						{
							if(group[heads[i] - 1] > 0)
							{
								if(group[i] != group[heads[i] - 1])
								{
									MergeGroup(group, group[i], group[heads[i] - 1], headGroup);
								}
							}
							else
							{
								group[heads[i] - 1] = group[i];
							}
						}
						else
						{
							group[i] = GetGroupId(group, heads[i] - 1, groupNum);
						}
					}
				}

				if(headGroup > 0)
				{
					for(int i = 0; i < len; ++i)
					{
						if(headGroup == group[i])
						{
							UTF82GBK(words[i].c_str(), str);
							result += str;
						}
					}
				}
			}

			cout << "转换前：" << line << endl;
			cout << "转换后：" << result << endl;
		}
	}
	fin.close();

	segmentor_release_segmentor(segEngine);
	postagger_release_postagger(posEngine);
	parser_release_parser(parseEngine);

	return 0;
}

int FindNextSubSentenceStart(string& line, const int startPos)
{
	int pos1 = line.find("；", startPos);
	int tempPos = line.find("。", startPos);
	if(-1 != tempPos && (tempPos < pos1 || -1 == pos1)) pos1 = tempPos;
	tempPos = line.find("：", startPos);
	if(-1 != tempPos && (tempPos < pos1 || -1 == pos1)) pos1 = tempPos;
	tempPos = line.find(":", startPos);
	if(-1 != tempPos && (tempPos < pos1 || -1 == pos1)) pos1 = tempPos;
	tempPos = line.find(";", startPos);
	if(-1 != tempPos && (tempPos < pos1 || -1 == pos1)) pos1 = tempPos;

	if(-1 == pos1) return -1;
	const int width = (line[pos1] == ';' || line[pos1] == ':')?1:2;
	pos1 += width;
	if(pos1 >= int(line.length())) return -1;
	return pos1;
}

int MergeGroup(vector <int>& group, const int fromId, const int toId, int& headGroup)
{
	if(headGroup == fromId)
	{
		headGroup = toId;
	}
	for(vector <int>::iterator iter = group.begin(); iter != group.end(); ++iter)
	{
		if(*iter == fromId)
		{
			*iter = toId;
		}
	}
	return toId;
}

int GetGroupId(vector <int>& group, const int wid, int& groupNum)
{
	if(int(group.size()) <= wid)
	{
		return 0;
	}
	if(0 < group[wid])
	{
		return group[wid];
	}
	else
	{
		++groupNum;
		group[wid] = groupNum;
		return groupNum;
	}
}

int GBK2UTF8(const char *szGbk, char *szUtf8)  
{  
	int n = MultiByteToWideChar(CP_ACP,0,szGbk,-1,NULL,0);  
	WCHAR *str1 = new WCHAR[sizeof(WCHAR) * n];  
	MultiByteToWideChar(CP_ACP,  // MultiByte的代码页Code Page  
		0,            //附加标志，与音标有关  
		szGbk,        // 输入的GBK字符串  
		-1,           // 输入字符串长度，-1表示由函数内部计算  
		str1,         // 输出  
		n             // 输出所需分配的内存  
		);  

	n = WideCharToMultiByte(CP_UTF8, 0, str1, -1, NULL, 0, NULL, NULL);    
	WideCharToMultiByte(CP_UTF8, 0, str1, -1, szUtf8, n, NULL, NULL);  
	delete[]str1;  
	str1 = NULL;  

	return 0;  
}

int UTF82GBK(const char *szUtf8, char *szGbk)  
{  
	int n = MultiByteToWideChar(CP_UTF8, 0, szUtf8, -1, NULL, 0);  
	WCHAR * wszGBK = new WCHAR[sizeof(WCHAR) * n];  
	memset(wszGBK, 0, sizeof(WCHAR) * n);  
	MultiByteToWideChar(CP_UTF8, 0,szUtf8,-1, wszGBK, n);  

	n = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, NULL, 0, NULL, NULL);  
	WideCharToMultiByte(CP_ACP,0, wszGBK, -1, szGbk, n, NULL, NULL);  

	delete[]wszGBK;  
	wszGBK = NULL;  

	return 0;  
}

