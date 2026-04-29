" 扩展 Lisp 语法：支持 #| ... |# 多行注释
syntax region lispMultilineComment1 start=/#|/ end=/|#/ contains=lispMultilineComment1
"syntax region lispMultilineComment2 start=/\/\*/ end=/\*\// contains=lispMultilineComment2
" 将多行注释区域设置为注释颜色
highlight link lispMultilineComment1 Comment
"highlight link lispMultilineComment2 Comment

" 高亮自定义原语
highlight link lispFunc Function

syntax match lispComment1 ";.*$" contains=@lispCommentGroup
syntax match lispComment2 "#.*$" contains=@lispCommentGroup
highlight link lispComment1 Comment
highlight link lispComment2 Comment
" 将括号单独高亮为红色
syntax match lispParenA "()"
syntax match lispParenB "\[\]"
syntax match lispParenC "{}"
highlight lispParenA ctermfg=red guifg=red
highlight lispParenB ctermfg=red guifg=red
highlight lispParenC ctermfg=red guifg=red

syntax keyword lispVar #t 
syntax keyword lispVar #f 
syntax keyword lispVar stdin
syntax keyword lispVar stdout
syntax keyword lispVar stderr
syntax keyword lispKey :p
syntax keyword lispVar nil
syntax keyword lispVar false
syntax keyword lispVar true
syntax keyword lispKey :q
syntax keyword lispFunc load

