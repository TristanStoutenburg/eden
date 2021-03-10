# eden
## building

I've been building using src/tool.c, if you compile tool using
```
cd src
gcc tool.c -o tool
./tool build game
./tool build platform
./tool run
```

Whih will run

```
gcc -g -c eden.c -o ../bin/eden.o                                  
gcc -g -shared ../bin/eden.o -o ../bin/eden.so                                      
rm ../bin/eden.o   
```

and

```
                          
gcc -g osx_main_eden.c -o ../bin/eden -framework OpenGL -l glew -l sdl2 -l sdl2_image -l sdl2_ttf -l sdl2_mixer      
rm -f ../bin/loop.edn      
```
 

and

```
./bin/eden
```

## description
building my own game from scratch. I'm following along handmade hero. Mainly doing this to learn about game engines.
