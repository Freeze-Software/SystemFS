# SystemFS
<img width="500" height="500" alt="SystemFS" src="https://github.com/user-attachments/assets/452b7de8-52e8-448b-870a-6b02b8e2dac9" />

build
'''bash
make 2>&1
'''

install
'''bash
git clone https://github.com/Freeze-Software/SystemFS.git
cd SystemFS

make

sudo make install
'''

use
'''bash
sfs archive.tar

sfs -k archive.tar

sfs -v archive.tar

sfs -d archive.tar.sfs

sfs -t archive.tar.sfs

sfs -dkv archive.tar.sfs
'''