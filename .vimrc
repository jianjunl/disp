
set paste
set backspace=indent,eol,start
set number

syntax on
" 识别 .lisp 文件为 lisp 类型
au BufRead,BufNewFile *.lisp set filetype=lisp
" 识别 .disp 文件为 lisp 类型
au BufRead,BufNewFile *.disp set filetype=lisp

" 当打开 lisp 文件时自动启用 lisp 模式
au FileType lisp setlocal lisp
" 当打开 disp 文件时自动启用 lisp 模式
au FileType disp setlocal lisp

" 括号匹配高亮
set showmatch
" 启用自动补全括号（可选）
inoremap ( ()<Left>
inoremap [ []<Left>
inoremap { {}<Left>
