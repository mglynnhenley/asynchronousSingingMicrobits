
import sys
with open('./main-tmp.cpp') as f:
  data= f.read()

with open(f'../tools/note{sys.argv[1]}.cpp') as f:
  note = f.read()

data = data.replace('XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX', note)
with open('./source/main.cpp', 'w') as f:
  f.write(data)

