import pdb;

def main():
  # get filename
  name = input("enter filename for output\n")
  
  # open file
  file = open(name, "a")

  # enter parameters
  start = int(input("enter starting address\n"),16)
  step  = int(input("enter step size\n"),       16)
  end   = int(input("enter end address\n"),     16)

  # entry
  # print("bpm " + "%X" % start)
  file.write("bpx " + "%X" % start + ",\"%X" % start + "\"\n")
  
  # determine size
  size = int(( end - start ) / step); 

  # iterate
  val = start
  for x in range(size):
    val = val + step
    # print("bpm " + "%X" % val)
    file.write("bpx " + "%X" % val + ",\"%X" % val + "\"\n")

  file.close()
  
main( )

