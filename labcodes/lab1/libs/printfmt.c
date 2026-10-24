/*
 * ============================================================================
 * libs/printfmt.c —— 格式化输出的"大脑"（lab1 最核心的算法文件，答辩重点）
 * ============================================================================
 * 【它解决什么问题】
 *   cprintf("hello %d, %s", 42, "abc") 里的 %d/%s 怎么变成真正的字符？
 *   这里完成：解析格式串 -> 按类型取参数 -> 逐字符输出。
 * 【设计上的高明之处：输出与设备解耦】
 *   本文件不知道字符最终去哪里，它只调用一个函数指针 putch(ch, putdat)：
 *     - 传 cputch + &cnt       -> 输出到屏幕（kern/libs/stdio.c 的 cprintf）
 *     - 传 sprintputch + &b    -> 输出到内存缓冲区（本文件的 snprintf）
 *   同一个格式化器服务两种输出目标，是答辩很容易加分的点。
 * 【依赖的基础设施】
 *   defs.h（类型/宏）、riscv.h（do_div 宏：一次除法同时得到商和余数）、
 *   error.h（%e 用的错误码表）、stdio.h（接口声明）、string.h（strnlen）。
 * 【支持的格式】
 *   %c 字符  %s 字符串  %d 有符号十进制  %u 无符号十进制  %o 八进制
 *   %p 指针  %x 十六进制  %e 错误码转字符串  %% 输出一个百分号
 *   可选修饰：- 左对齐  0 补零  数字 字段宽度  . 精度  # 转义  l 长整型
 * ============================================================================
 */
/* defs.h：定长类型、NULL 等基础定义。*/
#include <defs.h>
/* riscv.h：这里真正用到的是 do_div 宏（见下方 printnum 的注释）。*/
#include <riscv.h>
/* error.h：%e 格式符需要错误码宏。*/
#include <error.h>
/* stdio.h：本文件要实现的那几个函数（printfmt/vprintfmt/snprintf/vsnprintf）。*/
#include <stdio.h>
/* string.h：字符串工具，这里用 strnlen 计算输出宽度。*/
#include <string.h>

/*
 * 说明本文件支持的格式能力，以及 %e 的特殊语义：
 *   %e 接收一个整数错误码（如 -E_NO_MEM），打印它的文字说明，
 *   而且正负号都可以（内部先取绝对值），这样 -3 和 3 等价。
 */
/* *
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 *
 * The special format %e takes an integer error code
 * and prints a string describing the error.
 * The integer may be positive or negative,
 * so that -E_NO_MEM and E_NO_MEM are equivalent.
 * */

/* 错误码 -> 字符串的查表数组，下标就是错误码本身。
 *   [0] 是 NULL，所以 0 号错误码会被当成"未知错误"。
 *   用 [E_XXX] = "..." 的指定下标初始化语法，保证顺序错乱也不会串位。
 *   容量 MAXERROR+1 正好覆盖 0..MAXERROR。*/
static const char * const error_string[MAXERROR + 1] = {
    [0]                        NULL,
    [E_UNSPECIFIED]            "unspecified error",
    [E_BAD_PROC]            "bad process",
    [E_INVAL]                "invalid parameter",
    [E_NO_MEM]                "out of memory",
    [E_NO_FREE_PROC]        "out of processes",
    [E_FAULT]                "segmentation fault",
};

/*
 * printnum —— 把一个数字按任意进制打印出来（基数 <= 16）。
 * 【算法】先取最低位（do_div 同时算出商和余数），再递归处理高位，
 *   这样字符就按"从高位到低位"的顺序输出（递归天然实现逆序）。
 * 【形参】putch/putdat 是输出动作与其上下文；num 要打印的数；
 *   base 进制；width 最少输出多少位；padc 不够位时用什么字符填充。
 * 【为什么需要 padc 和 width】这样 %08x 才能打印成 0000002a。
 */
/* *
 * printnum - print a number (base <= 16) in reverse order
 * @putch:        specified putch function, print a single character
 * @putdat:        used by @putch function
 * @num:        the number will be printed
 * @base:        base for print, must be in [1, 16]
 * @width:         maximum number of digits, if the actual width is less than @width, use @padc instead
 * @padc:        character that padded on the left if the actual width is less than @width
 * */
static void
printnum(void (*putch)(int, void*), void *putdat,
        unsigned long long num, unsigned base, int width, int padc) {
    unsigned long long result = num;
/* do_div(result, base)：GCC 语句表达式宏，一次除法得到
 *   余数（作为宏的"返回值"）并把商写回 result。
 *   于是 mod 是最低位数字，result 是剩下的高位部分。*/
    unsigned mod = do_div(result, base);

/* 递归出口：如果还有更高位，先递归打印高位（宽度相应减 1）。*/
    // first recursively print all preceding (more significant) digits
/* 注意这里不能直接打印：必须先递归，才能保证高位在前。*/
    if (num >= base) {
        printnum(putch, putdat, result, base, width - 1, padc);
/* 已经是最低位了，那么在打印它之前先把缺少的位用 padc 补齐。*/
    } else {
        // print any needed pad characters before first digit
/* while (--width > 0)：前缀自减，因为当前这一位马上要自己打印。*/
        while (-- width > 0)
            putch(padc, putdat);
    }
    // then print this (the least significant) digit
/* 查表法：'0123456789abcdef' 的下标就是数字的值，天然支持 16 进制。*/
    putch("0123456789abcdef"[mod], putdat);
}

/*
 * getuint / getint —— 从变参列表里取出一个整数。
 * 【为什么需要两个版本】因为 C 的默认实参提升：int 会被提升，而
 *   有符号数必须单独用带符号类型取出来做符号扩展，否则负数会变成大正数。
 * 【lflag 的作用】printf 的 l / ll 修饰符决定实参的真实宽度：
 *   lflag==0 -> int，lflag==1 -> long，lflag>=2 -> long long。
 *   取错宽度会导致参数错位，后面所有输出全部乱掉 —— 这是手写 printf 的第一大坑。
 */
/* *
 * getuint - get an unsigned int of various possible sizes from a varargs list
 * @ap:            a varargs list pointer
 * @lflag:        determines the size of the vararg that @ap points to
 * */
static unsigned long long
getuint(va_list *ap, int lflag) {
/* ll 修饰（如 %lld）：取 8 字节。*/
    if (lflag >= 2) {
        return va_arg(*ap, unsigned long long);
    }
/* 单个 l（如 %ld）：取长整型。*/
    else if (lflag) {
        return va_arg(*ap, unsigned long);
    }
/* 没有修饰：按默认的 int 取。*/
    else {
        return va_arg(*ap, unsigned int);
    }
}

/* 与 getuint 同理，只是类型换成有符号版本。*/
/* *
 * getint - same as getuint but signed, we can't use getuint because of sign extension
 * @ap:            a varargs list pointer
 * @lflag:        determines the size of the vararg that @ap points to
 * */
static long long
getint(va_list *ap, int lflag) {
    if (lflag >= 2) {
        return va_arg(*ap, long long);
    }
    else if (lflag) {
        return va_arg(*ap, long);
    }
    else {
        return va_arg(*ap, int);
    }
}

/*
 * printfmt —— 可变参数版本的格式化输出，输出到 putch 指定的目标。
 * 它只做"把 ... 变成 va_list"，真正的活在 vprintfmt 里。
 */
/* *
 * printfmt - format a string and print it by using putch
 * @putch:        specified putch function, print a single character
 * @putdat:        used by @putch function
 * @fmt:        the format string to use
 * */
/* 这里的 ... 与 stdio.h 中的声明对应。*/
void
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
    va_list ap;

/* 打包可变参数 -> 交给 vprintfmt 统一处理。*/
    va_start(ap, fmt);
    vprintfmt(putch, putdat, fmt, ap);
    va_end(ap);
}

/*
 * vprintfmt —— 真正的格式化引擎（读这段代码就能讲清 lab1 的"输出"半边）。
 * 骨架：外层无限循环 -> 内层循环输出普通字符直到遇到 '%' ->
 *       switch 解析格式串 -> 输出对应内容 -> 回到外层。
 */
/* *
 * vprintfmt - format a string and print it by using putch, it's called with a va_list
 * instead of a variable number of arguments
 * @putch:        specified putch function, print a single character
 * @putdat:        used by @putch function
 * @fmt:        the format string to use
 * @ap:            arguments for the format string
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want printfmt() instead.
 * */
void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap) {
/* p 用于遍历格式串/字符串；ch 是当前字符；err 供 %e 使用。*/
    register const char *p;
/* register 是给编译器的提示：这几个变量访问频繁，尽量放寄存器。
 *   注意 num 用 unsigned long long，因为它是所有整型参数的统一"中转类型"。*/
    register int ch, err;
    unsigned long long num;
/* base 进制；width 字段宽度；precision 精度；
 *   lflag 记 l/ll 的个数；altflag 记 '#' 修饰。*/
    int base, width, precision, lflag, altflag;

    while (1) {
/* 【内层循环】逐字符输出，直到遇到 '%'。
 *   这里的 fmt++ 是后置自增：ch 取当前字符，指针再后移，所以
 *   遇到 '\0' 时恰好说明格式串输出完毕，直接 return 结束整个函数。*/
        while ((ch = *(unsigned char *)fmt ++) != '%') {
            if (ch == '\0') {
                return;
            }
            putch(ch, putdat);
        }

/* 每遇到一个 '%'，就重新初始化这五个修饰状态。
 *   为什么要放在这里？因为上一个格式符的修饰不能影响下一个。*/
        // Process a %-escape sequence
        char padc = ' ';
        width = precision = -1;
        lflag = altflag = 0;

/* reswitch 标签：修饰符可以连续出现（如 "%-08d"），
 *   所以每处理完一个修饰字符就 goto 回来继续 switch 下一个字符。*/
    reswitch:
        switch (ch = *(unsigned char *)fmt ++) {

/* '-'：左对齐，即先把内容输出、再用空格在右边补齐。*/
        // flag to pad on the right
        case '-':
            padc = '-';
            goto reswitch;

/* '0'：用 '0' 而不是空格做左填充（如 %05d -> 00042）。*/
        // flag to pad with 0's instead of spaces
        case '0':
            padc = '0';
            goto reswitch;

/* 数字开头：把连续的数字读成一个整数，作为"字段宽度"。
 *   例如 %12d 里读到 12，表示至少输出 12 个字符宽。*/
        // width field
        case '1' ... '9':
            for (precision = 0; ; ++ fmt) {
                precision = precision * 10 + ch - '0';
                ch = *fmt;
                if (ch < '0' || ch > '9') {
                    break;
                }
            }
            goto process_precision;

/* '*'：宽度/精度由参数给出（如 printf("%*d", 5, 42)）。*/
        case '*':
            precision = va_arg(ap, int);
            goto process_precision;

/* '.'：小数点，表示后面是精度。
 *   单独的 "." 等价于精度 0（例如 %.s 什么都不打印）。*/
        case '.':
            if (width < 0)
                width = 0;
            goto reswitch;

/* '#'：转义标志；对 %s 而言表示"不可打印字符输出成 ?"。*/
        case '#':
            altflag = 1;
            goto reswitch;

/* process_precision：宽度和精度其实是同一个数值的两种用法。
 *   标准 printf 里 "%.5d" 和 "%5d" 在 ucore 中被统一处理成宽度，
 *   所以这里的逻辑是：还没设置宽度时，把刚读到的数字当作宽度用。*/
        process_precision:
            if (width < 0)
                width = precision, precision = -1;
            goto reswitch;

/* 'l'：长整型修饰。出现一次是 long，出现两次是 long long，
 *   用 lflag++ 计数，取值时交给 getuint/getint 判断宽度。*/
        // long flag (doubled for long long)
        case 'l':
            lflag ++;
            goto reswitch;

/* 'c'：输出一个字符（变参默认提升为 int）。*/
        // character
        case 'c':
            putch(va_arg(ap, int), putdat);
            break;

/* 'e'：输出错误码对应的文字说明（ucore 特有，标准 printf 没有）。
 *   先取绝对值，再查 error_string[] 表；越界或为 NULL 时退化为 "error N"。*/
        // error message
        case 'e':
            err = va_arg(ap, int);
            if (err < 0) {
                err = -err;
            }
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
                printfmt(putch, putdat, "error %d", err);
            }
            else {
                printfmt(putch, putdat, "%s", p);
            }
            break;

/* 's'：输出字符串，支持精度截断和宽度对齐。
 *   先处理空指针（打印 "(null)"），保证内核不会因为 %s 挂了而崩掉。*/
        // string
        case 's':
            if ((p = va_arg(ap, char *)) == NULL) {
                p = "(null)";
/* NULL 字符串的兜底。*/
            }
/* 右对齐时先用 padc 把左侧补齐到宽度。*/
            if (width > 0 && padc != '-') {
                for (width -= strnlen(p, precision); width > 0; width --) {
                    putch(padc, putdat);
                }
            }
/* 逐个输出字符，precision < 0 表示"不限长度"；
 *   注意 --precision >= 0 的写法：精度用满就停止。*/
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
/* altflag 时把不可打印字符（含换行/制表）显示成 '?'，
 *   避免输出里的控制字符把屏幕搞乱。*/
                if (altflag && (ch < ' ' || ch > '~')) {
                    putch('?', putdat);
                }
                else {
                    putch(ch, putdat);
                }
            }
/* 左对齐时在右侧补空格。*/
            for (; width > 0; width --) {
                putch(' ', putdat);
            }
            break;

/* 'd'：有符号十进制。负数先输出 '-' 再取绝对值，
 *   然后跳到 number 标签统一交给 printnum 打印。*/
        // (signed) decimal
        case 'd':
            num = getint(&ap, lflag);
/* 注意先转成 (long long) 再比较，否则 num 是无符号的，永远不会小于 0。*/
            if ((long long)num < 0) {
                putch('-', putdat);
                num = -(long long)num;
            }
            base = 10;
            goto number;

/* 'u'：无符号十进制。*/
        // unsigned decimal
        case 'u':
            num = getuint(&ap, lflag);
            base = 10;
            goto number;

/* 'o'：八进制。*/
        // (unsigned) octal
        case 'o':
            num = getuint(&ap, lflag);
            base = 8;
            goto number;

/* 'p'：指针，先输出 "0x" 前缀再按十六进制打印数值。*/
        // pointer
        case 'p':
            putch('0', putdat);
            putch('x', putdat);
/* 指针是 64 位，必须用 void* 取出来再转成无符号整数。*/
            num = (unsigned long long)va_arg(ap, void *);
            base = 16;
            goto number;

/* 'x'：十六进制。*/
        // (unsigned) hexadecimal
        case 'x':
            num = getuint(&ap, lflag);
            base = 16;
/* number 标签：d/u/o/p/x 四种格式在这里汇合，
 *   统一交给 printnum 按 base 进制打印 —— 这就是"代码复用"。*/
        number:
            printnum(putch, putdat, num, base, width, padc);
            break;

/* '%%'：格式串里写 %% 就输出一个百分号。*/
        // escaped '%' character
        case '%':
            putch(ch, putdat);
            break;

/* default：遇到无法识别的格式符（如 %z），原样输出 '%' 和后面的字符，
 *   不丢内容。内层 for 循环把 fmt 退回到 '%' 之后，保证不丢字符。*/
        // unrecognized escape sequence - just print it literally
        default:
            putch('%', putdat);
            for (fmt --; fmt[-1] != '%'; fmt --)
                /* do nothing */;
            break;
        }
    }
}

/*
 * sprintbuf —— 描述"往一段内存缓冲区里写"的上下文。
 * 它就是 putdat 参数的真实类型（在 snprintf 里传 &b）。
 */
/* sprintbuf is used to save enough information of a buffer */
struct sprintbuf {
    char *buf;            // address pointer points to the first unused memory
    char *ebuf;            // points the end of the buffer
    int cnt;            // the number of characters that have been placed in this buffer
};

/* *
 * sprintputch - 'print' a single character in a buffer
 * @ch:            the character will be printed
 * @b:            the buffer to place the character @ch
 * */
/* sprintputch：与 cputch 同一接口（int, void*），但目标是缓冲区。
 *   关键的安全设计：cnt 无条件累加（表示"本应输出多少字符"），
 *   只有还没写满时才真正写入，写满就不再写 —— 于是缓冲区永不越界。*/
static void
sprintputch(int ch, struct sprintbuf *b) {
    b->cnt ++;
    if (b->buf < b->ebuf) {
        *b->buf ++ = ch;
    }
}

/*
 * snprintf —— 格式化结果写进缓冲区（而不是屏幕）。
 * size 包含结尾的 '\0'，返回值是"本该输出多少字符"（可用于判断被截断）。
 */
/* *
 * snprintf - format a string and place it in a buffer
 * @str:        the buffer to place the result into
 * @size:        the size of buffer, including the trailing null space
 * @fmt:        the format string to use
 * */
/* 与 cprintf 的套路完全一样：... -> va_list -> vsnprintf。*/
int
snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    int cnt;
    va_start(ap, fmt);
    cnt = vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return cnt;
}

/*
 * vsnprintf —— 真正干活的实现：用一个 sprintbuf 描述目标缓冲区，
 * 然后把 vprintfmt 的输出动作换成 sprintputch。
 * 这一段最能体现"格式化与输出解耦"的设计价值。
 */
/* *
 * vsnprintf - format a string and place it in a buffer, it's called with a va_list
 * instead of a variable number of arguments
 * @str:        the buffer to place the result into
 * @size:        the size of buffer, including the trailing null space
 * @fmt:        the format string to use
 * @ap:            arguments for the format string
 *
 * The return value is the number of characters which would be generated for the
 * given input, excluding the trailing '\0'.
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want snprintf() instead.
 * */
int
/* 与 printfmt/vprintfmt 的关系同构：一个接 ...，一个接 va_list。*/
vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
/* 初始化缓冲区上下文：
 *   buf  = 第一个可写入位置（起点）
 *   ebuf = 最后一个可写入位置（起点 + size - 1，留给结尾的 '\0'）
 *   cnt  = 已输出字符数，从 0 开始。*/
    struct sprintbuf b = {str, str + size - 1, 0};
/* 参数校验：str 为 NULL 或 size 为 0（此时 buf > ebuf）都返回 -E_INVAL。*/
    if (str == NULL || b.buf > b.ebuf) {
        return -E_INVAL;
    }
    // print the string to the buffer
/* 复用格式化引擎，只是把 putch 换成写缓冲区的 sprintputch。*/
    vprintfmt((void*)sprintputch, &b, fmt, ap);
    // null terminate the buffer
/* 补上字符串结束符 —— 这是 snprintf 的契约之一。*/
    *b.buf = '\0';
/* 返回"本应输出的字符数"（可能大于实际写入量，因为可能被截断）。*/
    return b.cnt;
}

