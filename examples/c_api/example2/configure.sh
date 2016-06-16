
# Where ld can find libluajack.so:
LibDir=/usr/local/lib

case :$LD_LIBRARY_PATH: in
 *:$LibDir:*) ;; # already in
 *) export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LibDir;;
esac


