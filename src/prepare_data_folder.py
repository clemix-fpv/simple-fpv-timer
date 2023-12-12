# SPDX-License-Identifier: GPL-3.0+

Import('env')

import os
import gzip
import shutil
from pathlib import Path
import glob
import subprocess

def prepare_www_files(source, target, env):

    filetypes_to_gzip = ['js', 'html', 'css']

    proj_dir = Path(env.get("PROJECT_DIR"))
    data_src_dir = os.path.join(proj_dir, 'data_src')
    data_dir = os.path.join(proj_dir, 'data')

    if(os.path.exists(data_dir)):
        print('  Delete data dir {}'.format(data_dir))
        shutil.rmtree(data_dir)

    print('  Re-creating empty data dir {} '.format(data_dir))
    os.mkdir(data_dir)

    files_to_gzip = []
    for extension in filetypes_to_gzip:
        files_to_gzip.extend(glob.glob(os.path.join(data_src_dir, '*.' + extension)))
    
    print('  files to gzip: ' + str(files_to_gzip))

    all_files = glob.glob(os.path.join(data_src_dir, '*.*'))
    files_to_copy = list(set(all_files) - set(files_to_gzip))

    print('  files to copy: ' + str(files_to_copy))

    for file in files_to_copy:
        print('  COPY: ' + file)
        shutil.copy(file, data_dir)
    
    for file in files_to_gzip:
        dst = os.path.join(data_dir, os.path.basename(file) + '.gz')
        print('  GZIP: ' + file + ' -> ' + dst)
        cmd = 'gzip -9 < {S} > {D}'.format(S=file, D=dst)
        subprocess.check_call(cmd, shell=True)


env.AddPreAction('$BUILD_DIR/spiffs.bin', prepare_www_files)
