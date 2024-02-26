VPATH := ../Coroutine
prom = testCoroutine
src = testCoroutine.cpp Coroutine.cpp
obj = $(src:.cpp=.o)  # 将源文件转换为目标文件

$(prom): $(obj)
	g++ $^ -o $@

%.o: %.cpp
	g++ -c $< -o $@

# 设置依赖关系
testCoroutine.o: testCoroutine.cpp Coroutine.h
Coroutine.o: Coroutine.cpp Coroutine.h

.PHONY: clean
clean:
	rm -f $(obj) $(prom)
