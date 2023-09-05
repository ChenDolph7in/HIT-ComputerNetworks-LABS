# 实验一

## proxy_server.cpp	

c++源代码



## banned_client.txt

放置过滤用户的txt文件，文件中每行一个被禁用的用户IP地址。

在源代码中使用GetBanned_client()读取用户，参数为文件名。

```c++
void GetBanned_client(const char* filename)
{
	ifstream in(filename);
	string line;

	if (in) // 有该文件  
	{
		while (getline(in, line)) // line中不包括每行的换行符  
		{
			banned_client.push_front(line);
		}
	}
	else // 没有该文件  
	{
		cout << "no such file" << endl;
	}
}
```



## banned_server.txt

放置过滤网站的txt文件，文件中每行一个被禁用的网站域名。

在源代码中使用GetBanned_server()读取网站，参数为文件名。

```c++
void GetBanned_server(const char* filename) {
	ifstream in(filename);
	string line;

	if (in) // 有该文件  
	{
		while (getline(in, line)) // line中不包括每行的换行符  
		{
			banned_server.push_front(line);
		}
	}
	else // 没有该文件  
	{
		cout << "no such file" << endl;
	}
}
```



## fish.txt

放置被钓鱼的网站的txt文件，文件中每行一个被引导的网站域名。

在源代码中使用GetFish()读取被钓鱼的网站，参数为文件名。

```c++
void GetFish(const char* filename) {
	ifstream in(filename);
	string line;
	if (in) // 有该文件  
	{
		while (getline(in, line)) // line中不包括每行的换行符  
		{
			fish.push_front(line);
		}
	}
	else // 没有该文件  
	{
		cout << "no such file" << endl;
	}
}
```

