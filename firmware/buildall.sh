boards="chroma29r chroma74r chroma74y EPOP50 EPOP900 zbs29v025 zbs29v026"

if [ -e build.log ]; then
   rm build.log
fi

for board in $boards; do
  echo "Building $board"
  make BOARD=$board clean >> build.log 2>&1
  make BOARD=$board >> build.log 2>&1
  if [ $? -ne 0 ];then
     echo "Build failed building $board"
     tail build.log
     exit 1
  fi
done

