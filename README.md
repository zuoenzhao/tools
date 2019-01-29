# tools
网络线程等实例代码

1：pingPoll.c
    轮询方式ping某个ip地址或者主机名，检测网络是否连接外网，比如ping www.baidu.com
    pingPoll [www.baidu.com] [5] [3000]   这里ping百度，icmp包发送五次，并且3s的超时设置，如果网络不通，3s后会返回，并且API会返回从icmp包发送到接收完毕用时，单位为s.
    
2:pingpthread.c
  多线程方式进行ping程序的发送和接收，功能同pingPoll.c相似，但是多线程要多占用内存空间。
