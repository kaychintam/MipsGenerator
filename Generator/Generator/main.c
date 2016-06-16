#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//定义布尔类型
typedef enum {FALSE, TRUE} bool;

//定义变量类型
typedef enum {INTEGER, FLOAT, DOUBLE, STRING, ARRAY, UNDEFINED, IMM} type;

//变量
typedef struct {
    char name[200];
    union {
        char initval[200]; //初始值
        char floatval[200];
        char doubleval[200];
    };
    type type; //指向的类型
} Variable;

//变量表
typedef struct {
    int num; //变量数
    Variable var[200];
} VariableMap;

//定义三地址码的操作类型
typedef enum {
    gt, sm, eq, if_f, lab, add, sub, dvd, mul, rd, pri, priln, asn, jump, dec, call
    //add sub mul dvd是加减乘除
    //gt sm eq是比较
    //pri priln rd是输入输出
    //if_f jump是跳转
    //dec改变指针类型
    //func是函数
} OpKind;

//定义三地址码的地址类型 分别是空的、整形、字符串类型（名字）、地址（含&）、值（含*）
typedef enum {
    IntConst, FloatConst, String, Addr, Val, Empty
} AddrKind;

//定义三地址码的地址结构
typedef struct {
    AddrKind kind;
    union {
        int intVal;
        float floatVal;
        double doubleVal;
        char name[20];
    } contents;
} Address;

//定义三地址码的语句格式 即四元组
typedef struct {
    Address addr1, addr2, addr3;
    OpKind op;
} Quad;

//四元组表
typedef struct {
    int num;
    Quad quad[1000];
} QuadTable;

//标签表
typedef struct {
    int num;
    char labels[100][100];
} LabelTable;

FILE* fp;
FILE* out;

//从一行三地址码中提取出元素,成功则为1，失败则为0
//src是源地址码 dst是提取出的元素 head是从src的某个位置开始扫描
int getElement(char* src, int* head, char* dst) {
    int p = *head;
    int cnt = 0;
    while (src[p]!='\0') {
        if (src[p]==' ') break;
        dst[cnt++] = src[p++];
    }
    if (src[p]==' ') p++;
    *head = p;
    return cnt>0;
}

//添加一个变量到变量表中
void addVariable(VariableMap* map, char* name, type vartype, char* initval) {
    strcpy(map->var[map->num].name, name);
    strcpy(map->var[map->num].initval, initval);
    map->var[map->num].type = vartype;
    map->num++;
}

//添加一个数组到变量表中
void addArray(VariableMap* map, char* name, int space) {
    strcpy(map->var[map->num].name, name);
    map->var[map->num].type = ARRAY;
//    map->var[map->num].space = space;
    map->num++;
}

//添加一个标签
void addLabel(char* name, LabelTable* lb) {
    strcpy(lb->labels[lb->num], name);
    lb->num++;
}

//根据字符串判断元素的类型（已处理浮点数）
AddrKind getKind(char* name) {
    if (name[0]=='&') return Addr;
    if (name[0]=='*') return Val;
    if (name[0]=='_' || (name[0]>='a' && name[0]<='z') || (name[0]>='A' && name[0]<='Z')) return String;
    if (strstr(name, ".")>0) return FloatConst;
    if (name[0]>='0' && name[0]<='9') return IntConst;
    return Empty;
}

//根据变量名查到到它的类型
type getType(char* name, VariableMap* map) {
    if (name[0]=='$') return INTEGER;
    for (int i=0; i<map->num; i++)
        if (strcmp(name, map->var[i].name)==0)
            return map->var[i].type;
    return UNDEFINED;
}

//改变某个变量指针指向的类型
void changeType(Quad* quad, VariableMap* map) {
    type t = UNDEFINED;
    if (strcmp(quad->addr1.contents.name, "int")==0) t = INTEGER;
    if (strcmp(quad->addr1.contents.name, "float")==0) t = FLOAT;
    if (strcmp(quad->addr1.contents.name, "double")==0) t = DOUBLE;
    for (int i=0; i<map->num; i++)
        if (strcmp(map->var[i].name, quad->addr2.contents.name) == 0) {
            map->var[i].type = t;
            return;
        }
}

//定义所有的变量 (已支持浮点数）
void declareVariable(VariableMap* map) {
    int i;
    fprintf(out, ".data 0x10000000\n\n");
    fprintf(out, "\tendl:\t.asciiz\t\"\\n\"\n");
    for (i=0; i<map->num; i++) {
        if (map->var[i].type == INTEGER)
            fprintf(out, "\t%s:\t.word\t%s\n", map->var[i].name, map->var[i].initval);
        if (map->var[i].type == FLOAT)
            fprintf(out, "\t%s:\t.float\t%s\n", map->var[i].name, map->var[i].floatval);
        if (map->var[i].type == ARRAY)
            fprintf(out, "\t%s:\t.double\t%s\n", map->var[i].name, map->var[i].doubleval);
    }
    fprintf(out, "\n");
}

//将赋值的三地址码语句转化为mips（已支持浮点数）
void genASN(Quad* quad, VariableMap* map) {
    fprintf(out, "\t# assign\n");
    if (quad->addr2.kind == String) {
        if (quad->addr1.kind == IntConst) fprintf(out, "\tli $t1, %d\n", quad->addr1.contents.intVal);
        if (quad->addr1.kind == String) fprintf(out, "\tlw $t1, %s\n", quad->addr1.contents.name);
        fprintf(out, "\tsw $t1, %s\n", quad->addr2.contents.name);
    }
    if (quad->addr2.kind == Val) {
        type t2 = getType(quad->addr2.contents.name+1, map);
        if (t2 == INTEGER) {
            if (quad->addr1.kind == IntConst) fprintf(out, "\tli $t0, %d\n", quad->addr1.contents.intVal);
            if (quad->addr1.kind == Val) {
                fprintf(out, "\tlw $t0, %s\n", quad->addr1.contents.name+1);
                fprintf(out, "\tlw $t0, 0($t0)\n");
            }
            fprintf(out, "\tlw $t1, %s\n", quad->addr2.contents.name+1);
            fprintf(out, "\tsw $t0, 0($t1)\n");
        }
        if (t2 == FLOAT) {
            if (quad->addr1.kind == IntConst) {
                fprintf(out, "\tli $t0, %d\n", quad->addr1.contents.intVal);
                fprintf(out, "\tmtc1, $t0, $f0\n");
                fprintf(out, "\tcvt.s.w $f0, $f0\n");
            }
            if (quad->addr1.kind == FloatConst) fprintf(out, "\tli.s $f0, %f\n", quad->addr1.contents.floatVal);
            if (quad->addr1.kind == Val) {
                fprintf(out, "\tlw $t0, %s\n", quad->addr1.contents.name+1);
                if (getType(quad->addr1.contents.name+1, map)==FLOAT) fprintf(out, "\tl.s $f0, 0($t0)\n");
                if (getType(quad->addr1.contents.name+1, map)==INTEGER) {
                    fprintf(out, "\tlw $t0, 0($t0)\n");
                    fprintf(out, "\tmtc1, $t0, $f0\n");
                    fprintf(out, "\tcvt.s.w $f0, $f0\n");
                }
            }
            fprintf(out, "\tlw $t0, %s\n", quad->addr2.contents.name+1);
            fprintf(out, "\ts.s $f0, 0($t0)\n");
        }
        if (t2 == DOUBLE) {
            if (quad->addr1.kind == IntConst) {
                fprintf(out, "\tli $t0, %d\n", quad->addr1.contents.intVal);
                fprintf(out, "\tmtc1, $t0, $f0\n");
                fprintf(out, "\tcvt.s.w $f0, $f0\n");
                fprintf(out, "\tcvt.d.s $f0, $f0\n");
            }
            if (quad->addr1.kind == FloatConst) {
                fprintf(out, "\tli.s $f0, %f\n", quad->addr1.contents.floatVal);
                fprintf(out, "\tcvt.d.s, $f0, $f0\n");
            }
            if (quad->addr1.kind == Val) {
                fprintf(out, "\tlw $t0, %s\n", quad->addr1.contents.name+1);
                if (getType(quad->addr1.contents.name+1, map)==FLOAT) {
                    fprintf(out, "\tl.s $f0, 0($t0)\n");
                    fprintf(out, "\tcvt.d.s, $f0, $f0");
                }
                if (getType(quad->addr1.contents.name+1, map)==INTEGER) {
                    fprintf(out, "\tlw $t0, 0($t0)\n");
                    fprintf(out, "\tmtc1 $t0, $f0\n");
                    fprintf(out, "\tcvt.s.w $f0, $f0\n");
                    fprintf(out, "\tcvt.d.s $f0, $f0\n");
                }
                if (getType(quad->addr1.contents.name+1, map)==DOUBLE) fprintf(out, "\tl.d $f0, 0($t0)\n");
            }
            fprintf(out, "\tlw $t0, %s\n", quad->addr2.contents.name+1);
            fprintf(out, "\ts.d $f0, 0($t0)\n");
        }
    }
}

//将加、减、乘法的三地址码语句转化为mips（已支持浮点数）
void genCAL(Quad* quad, VariableMap* map) {
    fprintf(out, "\t# calculation\n");
    type t1 = UNDEFINED, t2 = UNDEFINED, t3 = UNDEFINED;
    if (quad->addr3.kind == String) { //关于地址的计算
        if (quad->addr1.kind == IntConst) fprintf(out, "\tli $t1, %d\n", quad->addr1.contents.intVal);
        if (quad->addr1.kind == String) fprintf(out, "\tlw $t1, %s\n", quad->addr1.contents.name);
        if (quad->addr2.kind == IntConst) fprintf(out, "\tli $t2, %d\n", quad->addr2.contents.intVal);
        if (quad->addr2.kind == String) fprintf(out, "\tlw $t2, %s\n", quad->addr2.contents.name);
        if (quad->op == add) fprintf(out, "\tadd $t1, $t1, $t2\n");
        if (quad->op == sub) fprintf(out, "\tsub $t1, $t1, $t2\n");
        fprintf(out, "\tsw $t1, %s\n", quad->addr3.contents.name);
    }
    if (quad->addr3.kind == Val) { //关于值的计算
        t3 = getType(quad->addr3.contents.name+1, map);
        if (t3 == INTEGER) {
            if (quad->addr1.kind == IntConst) fprintf(out, "\tli $t1, %d\n", quad->addr1.contents.intVal);
            if (quad->addr1.kind == Val) {
                fprintf(out, "\tlw $t3, %s\n", quad->addr1.contents.name+1);
                fprintf(out, "\tlw $t1, 0($t3)\n");
            }
            if (quad->addr2.kind == IntConst) fprintf(out, "\tli $t2, %d\n", quad->addr2.contents.intVal);
            if (quad->addr2.kind == Val) {
                fprintf(out, "\tlw $t3, %s\n", quad->addr2.contents.name+1);
                fprintf(out, "\tlw $t2, 0($t3)\n");
            }
            if (quad->op == add) fprintf(out, "\tadd $t1, $t1, $t2\n");
            if (quad->op == sub) fprintf(out, "\tsub $t1, $t1, $t2\n");
            if (quad->op == mul) fprintf(out, "\tmul $t1, $t1, $t2\n");
            if (quad->op == dvd) fprintf(out, "\tdiv $t1, $t2\n");
            if (quad->op != dvd) {
                fprintf(out, "\tlw $t3, %s\n", quad->addr3.contents.name+1);
                fprintf(out, "\tsw $t1, 0($t3)\n");
            }
            if (quad->op == dvd) {
                fprintf(out, "\tlw $t3, %s\n", quad->addr3.contents.name+1);
                fprintf(out, "\tmflo $t2\n");
                fprintf(out, "\tsw $t2 0($t3)\n");
            }
        }
        if (t3 == FLOAT) {
            if (quad->addr1.kind == IntConst) fprintf(out, "\tli.s $f1, %d.0\n", quad->addr1.contents.intVal);
            if (quad->addr1.kind == FloatConst) fprintf(out, "\tli.s $f1, %f\n", quad->addr1.contents.floatVal);
            if (quad->addr1.kind == Val) {
                t1 = getType(quad->addr1.contents.name+1, map);
                fprintf(out, "\tlw $t3, %s\n", quad->addr1.contents.name+1);
                if (t1 == FLOAT) fprintf(out, "\tl.s $f1, 0($t3)\n");
                if (t1 == INTEGER) {
                    fprintf(out, "lw $t1, 0($t3)\n");
                    fprintf(out, "\tmtc1 $t1, $f1\n");
                    fprintf(out, "\tcvt.s.w $f1, $f1\n");
                }
            }
            if (quad->addr2.kind == IntConst) fprintf(out, "\tli.s $f2, %d.0\n", quad->addr2.contents.intVal);
            if (quad->addr2.kind == FloatConst) fprintf(out, "\tli.s $f2, %f\n", quad->addr2.contents.floatVal);
            if (quad->addr2.kind == Val) { //要强制转换类型
                t2 = getType(quad->addr2.contents.name+1, map);
                fprintf(out, "\tlw $t3, %s\n", quad->addr2.contents.name+1);
                if (t2 == FLOAT) fprintf(out, "\tl.s $f2, 0($t3)\n");
                if (t2 == INTEGER) {
                    fprintf(out, "\tlw $t2, 0($t3)\n");
                    fprintf(out, "\tmtc1 $t2, $f2\n");
                    fprintf(out, "\tcvt.s.w $f2, $f2\n");
                }
            }
            if (quad->op == add) fprintf(out, "\tadd.s $f3, $f1, $f2\n");
            if (quad->op == sub) fprintf(out, "\tsub.s $f3, $f1, $f2\n");
            if (quad->op == mul) fprintf(out, "\tmul.s $f3, $f1, $f2\n");
            if (quad->op == dvd) fprintf(out, "\tdiv.s $f3, $f1, $f2\n");
            fprintf(out, "\tlw $t3, %s\n", quad->addr3.contents.name+1);
            fprintf(out, "\ts.s $f3, 0($t3)\n");
        }
        if (t3 == DOUBLE) {
            if (quad->addr1.kind == IntConst) fprintf(out, "\tli.d $f0, %d.0\n", quad->addr1.contents.intVal);
            if (quad->addr1.kind == FloatConst) fprintf(out, "\tli.d $f0, %f\n", quad->addr1.contents.floatVal);
            if (quad->addr1.kind == Val) {
                t1 = getType(quad->addr1.contents.name+1, map);
                fprintf(out, "\tlw $t3, %s\n", quad->addr1.contents.name+1);
                if (t1 == INTEGER) {
                    fprintf(out, "\tlw $t1, 0($t3)\n");
                    fprintf(out, "\tmtc1 $t1, $f0\n");
                    fprintf(out, "\tcvt.s.w $f0, $f0\n");
                    fprintf(out, "\tcvt.d.s $f0, $f0\n");
                }
                if (t1 == FLOAT) {
                    fprintf(out, "\tl.s $f0, 0($t3)\n");
                    fprintf(out, "\tcvt.d.s $f0, $f0\n");
                }
                if (t1 == DOUBLE) fprintf(out, "\tl.d $f0, 0($t3)\n");
            }
            if (quad->addr2.kind == IntConst) fprintf(out, "\tli.d $f2, %d.0\n", quad->addr2.contents.intVal);
            if (quad->addr2.kind == FloatConst) fprintf(out, "\tli.d $f2, %f\n", quad->addr2.contents.floatVal);
            if (quad->addr2.kind == Val) {
                t2 = getType(quad->addr2.contents.name+1, map);
                fprintf(out, "\tlw $t3, %s\n", quad->addr2.contents.name+1);
                if (t2 == INTEGER) {
                    fprintf(out, "\tlw $t2, 0($t3)\n");
                    fprintf(out, "\tmtc1 $t2, $f2\n");
                    fprintf(out, "\tcvt.s.w $f2, $f2\n");
                    fprintf(out, "\tcvt.d.s $f2, $f2\n");
                }
                if (t2 == FLOAT) {
                    fprintf(out, "\tl.s $f2, 0($t3)\n");
                    fprintf(out, "\tcvt.d.s $f2, $f2\n");
                }
                if (t2 == DOUBLE) fprintf(out, "\tl.d $f2, 0($t3)\n");
            }
            if (quad->op == add) fprintf(out, "\tadd.d $f4, $f0, $f2\n");
            if (quad->op == sub) fprintf(out, "\tsub.d $f4, $f0, $f2\n");
            if (quad->op == mul) fprintf(out, "\tmul.d $f4, $f0, $f2\n");
            if (quad->op == dvd) fprintf(out, "\tdiv.d $f4, $f0, $f2\n");
            fprintf(out, "\tlw $t3, %s\n", quad->addr3.contents.name+1);
            fprintf(out, "\ts.d $f4, 0($t3)\n");
        }
    }
}

//生成读入的句子（已支持浮点数）
void genREAD(Quad* quad, VariableMap* map) {
    fprintf(out, "\t# read\n");
//    保存现场
//    fprintf(out, "\taddi $sp, $sp, -4\n");
//    fprintf(out, "\tsw $ra, 0($sp)\n");
    type t = getType(quad->addr1.contents.name, map);
    if (t == INTEGER) {
        fprintf(out, "\tli $v0, 5\n");
        fprintf(out, "\tsyscall\n");
        fprintf(out, "\tlw $t0, %s\n", quad->addr1.contents.name);
        fprintf(out, "\tsw $v0, 0($t0)\n");
    }
    if (t == FLOAT) {
        fprintf(out, "\tli $v0, 6\n");
        fprintf(out, "\tsyscall\n");
        fprintf(out, "\tlw $t0, %s\n", quad->addr1.contents.name);
        fprintf(out, "\ts.s $f0, 0($t0)\n");
    }
    if (t == DOUBLE) {
        fprintf(out, "\tli $v0, 7\n");
        fprintf(out, "\tsyscall\n");
        fprintf(out, "\tlw $t0, %s\n", quad->addr1.contents.name);
        fprintf(out, "\ts.d $f0, 0($t0)\n");
    }
//    恢复现场
//    fprintf(out, "\tlw $ra, 0($sp)\n");
//    fprintf(out, "\taddi $sp, $sp, 4\n");
}

//生成输出的句子（已支持浮点数）
void genPRINT(Quad* quad, VariableMap* map) {
    fprintf(out, "\t# print\n");
//    保存现场
//    fprintf(out, "\taddi $sp, $sp, -4\n");
//    fprintf(out, "\tsw $ra, 0($sp)\n");
    fprintf(out, "\tlw $t0, %s\n", quad->addr1.contents.name);
    type t = getType(quad->addr1.contents.name, map);
    if (t == INTEGER) {
        fprintf(out, "\tlw $a0, 0($t0)\n");
        fprintf(out, "\tli $v0, 1\n");
        fprintf(out, "\tsyscall\n");
    }
    if (t == FLOAT) {
        fprintf(out, "\tl.s $f12, 0($t0)\n");
        fprintf(out, "\tli $v0, 2\n");
        fprintf(out, "\tsyscall\n");
    }
    if (t == DOUBLE) {
        fprintf(out, "\tl.d $f12, 0($t0)\n");
        fprintf(out, "\tli $v0, 3\n");
        fprintf(out, "\tsyscall\n");
    }
    if (quad->op == priln) {
        fprintf(out, "\tla $a0, endl\n");
        fprintf(out, "\tli $v0, 4\n");
        fprintf(out, "\tsyscall\n");
    }
//    恢复现场
//    fprintf(out, "\tlw $ra, 0($sp)\n");
//    fprintf(out, "\taddi $sp, $sp, 4\n");
}

//标一个标签
void genLABEL(Quad* quad) {
    fprintf(out, "%s:\n", quad->addr1.contents.name);
}

//生成if_false跳转的语句
void genFJUMP(Quad* quad) {
    fprintf(out, "\t# if_false jump\n");
    if (strcmp(quad->addr1.contents.name, "$t7")==0) {
        fprintf(out, "\tadd $t1, $t7, $0\n");
    } else {
        fprintf(out, "\tlw $t1, %s\n", quad->addr1.contents.name);
        fprintf(out, "\tlw $t1, 0($t1)\n");
    }
    fprintf(out, "\tbne $t1, $0, %s\n", quad->addr2.contents.name);
}

//直接goto
void genJUMP(Quad* quad) {
    fprintf(out, "\t# direct jump\n");
    fprintf(out, "\tj %s\n", quad->addr1.contents.name);
}

//生成比较的语句,结果只能为INTEGER？
void genCompare(Quad* quad, VariableMap* map) {
    fprintf(out, "\t# compare\n");
    type t1 = UNDEFINED, t2 = UNDEFINED, t=UNDEFINED;
    if (quad->addr1.kind==Val) t1 = getType(quad->addr1.contents.name+1, map); else
        switch (quad->addr1.kind) {
            case IntConst: t1 = INTEGER; break;
            case FloatConst: t1 = FLOAT; break;
            default: break;
        }
    if (quad->addr2.kind==Val) t2 = getType(quad->addr2.contents.name+1, map); else
        switch (quad->addr2.kind) {
            case IntConst: t2 = INTEGER; break;
            case FloatConst: t2 = FLOAT; break;
            default: break;
        }
    if (t1==DOUBLE || t2 == DOUBLE)
        t=DOUBLE; else
    if (t1==FLOAT || t2 == FLOAT)
        t = FLOAT; else
    if (t1==INTEGER && t2==INTEGER)
        t = INTEGER;
    if (t == INTEGER) { //如果 a = b op c的b和c都是整形
        if (quad->addr1.kind == IntConst) fprintf(out, "\tli $t1, %d\n", quad->addr1.contents.intVal);
        if (quad->addr1.kind == Val) {
            fprintf(out, "\tlw $t3, %s\n", quad->addr1.contents.name+1);
            fprintf(out, "\tlw $t1, 0($t3)\n");
        }
        if (quad->addr2.kind == IntConst) fprintf(out, "\tli $t2, %d\n", quad->addr2.contents.intVal);
        if (quad->addr2.kind == Val) {
            fprintf(out, "\tlw $t3, %s\n", quad->addr2.contents.name+1);
            fprintf(out, "\tlw $t2, 0($t3)\n");
        }
        if (quad->op == sm) fprintf(out, "\tslt $t3, $t1, $t2\n"); //如果c=a<b且a确实小于b则t0=1否则t0=0
        if (quad->op == gt) fprintf(out, "\tslt $t3, $t2, $t1\n");
        if (quad->op == eq) {
            fprintf(out, "\tslt $t4, $t1, $t2\n");
            fprintf(out, "\tslt $t5, $t2, $t1\n");
            fprintf(out, "\tnor $t3, $t4, $t5\n");
            fprintf(out, "\tandi $t3, $t3, 1\n");
        }
    }
    if (t == FLOAT) {
        if (quad->addr1.kind == IntConst) fprintf(out, "\tli.s $f0, %d.0\n", quad->addr1.contents.intVal);
        if (quad->addr1.kind == FloatConst) fprintf(out, "\tli.s $f0, %f\n", quad->addr1.contents.floatVal);
        if (quad->addr1.kind == Val) {
            t1 = getType(quad->addr1.contents.name+1, map);
            fprintf(out, "\tlw $t3, %s\n", quad->addr1.contents.name+1);
            if (t1 == FLOAT) fprintf(out, "\tl.s $f0, 0($t3)\n");
            if (t1 == INTEGER) {
                fprintf(out, "lw $t1, 0($t3)\n");
                fprintf(out, "\tmtc1 $t1, $f0\n");
                fprintf(out, "\tcvt.s.w $f0, $f0\n");
            }
        }
        if (quad->addr2.kind == IntConst) fprintf(out, "\tli.s $f2, %d.0\n", quad->addr2.contents.intVal);
        if (quad->addr2.kind == FloatConst) fprintf(out, "\tli.s $f2, %f\n", quad->addr2.contents.floatVal);
        if (quad->addr2.kind == Val) { //要强制转换类型
            t2 = getType(quad->addr2.contents.name+1, map);
            fprintf(out, "\tlw $t3, %s\n", quad->addr2.contents.name+1);
            if (t2 == FLOAT) fprintf(out, "\tl.s $f2, 0($t3)\n");
            if (t2 == INTEGER) {
                fprintf(out, "\tlw $t2, 0($t3)\n");
                fprintf(out, "\tmtc1 $t2, $f2\n");
                fprintf(out, "\tcvt.s.w $f2, $f2\n");
            }
        }
        if (quad->op == sm) fprintf(out, "\tc.lt.s 0, $f0, $f2\n");
        if (quad->op == gt) fprintf(out, "\tc.lt.s 0, $f2, $f0\n");
        if (quad->op == eq) fprintf(out, "\tc.eq.s 0, $f0, $f2\n");
        fprintf(out, "\tli $t5, 1\n");
        fprintf(out, "\tli, $t3, 0\n");
        fprintf(out, "\tmovt $t3, $t5, 0\n");
    }
    if (t == DOUBLE) {
        if (quad->addr1.kind == IntConst) fprintf(out, "\tli.d $f0, %d.0\n", quad->addr1.contents.intVal);
        if (quad->addr1.kind == FloatConst) fprintf(out, "\tli.d $f0, %f\n", quad->addr1.contents.floatVal);
        if (quad->addr1.kind == Val) {
            t1 = getType(quad->addr1.contents.name+1, map);
            fprintf(out, "\tlw $t3, %s\n", quad->addr1.contents.name+1);
            if (t1 == INTEGER) {
                fprintf(out, "\tlw $t1, 0($t3)\n");
                fprintf(out, "\tmtc1 $t1, $f0\n");
                fprintf(out, "\tcvt.s.w $f0, $f0\n");
                fprintf(out, "\tcvt.d.s $f0, $f0\n");
            }
            if (t1 == FLOAT) {
                fprintf(out, "\tl.s $f0, 0($t3)\n");
                fprintf(out, "\tcvt.d.s $f0, $f0\n");
            }
            if (t1 == DOUBLE) fprintf(out, "\tl.d $f0, 0($t3)\n");
        }
        if (quad->addr2.kind == IntConst) fprintf(out, "\tli.d $f2, %d.0\n", quad->addr2.contents.intVal);
        if (quad->addr2.kind == FloatConst) fprintf(out, "\tli.d $f2, %f\n", quad->addr2.contents.floatVal);
        if (quad->addr2.kind == Val) {
            t2 = getType(quad->addr2.contents.name+1, map);
            fprintf(out, "\tlw $t3, %s\n", quad->addr2.contents.name+1);
            if (t2 == INTEGER) {
                fprintf(out, "\tlw $t2, 0($t3)\n");
                fprintf(out, "\tmtc1 $t2, $f2\n");
                fprintf(out, "\tcvt.s.w $f2, $f2\n");
                fprintf(out, "\tcvt.d.s $f2, $f2\n");
            }
            if (t2 == FLOAT) {
                fprintf(out, "\tl.s $f2, 0($t3)\n");
                fprintf(out, "\tcvt.d.s $f2, $f2\n");
            }
            if (t2 == DOUBLE) fprintf(out, "\tl.d $f2, 0($t3)\n");
        }
        if (quad->op == sm) fprintf(out, "\tc.lt.d 0, $f0, $f2\n");
        if (quad->op == gt) fprintf(out, "\tc.lt.d 0, $f2, $f0\n");
        if (quad->op == eq) fprintf(out, "\tc.eq.d 0, $f0, $f2\n");
        fprintf(out, "\tli $t5, 1\n");
        fprintf(out, "\tli, $t3, 0\n");
        fprintf(out, "\tmovt $t3, $t5, 0\n");
    }
    if (strcmp(quad->addr3.contents.name, "$t7")==0) {
        fprintf(out, "\tadd $t7, $t3, $0\n");
        return;
    }
    t = getType(quad->addr3.contents.name+1, map);
    fprintf(out, "\tlw $t2, %s\n", quad->addr3.contents.name+1);
    if (t == INTEGER) fprintf(out, "\tsw $t3, 0($t2)\n");
    if (t == FLOAT) {
        fprintf(out, "\tmtc1 $t3, $f0\n");
        fprintf(out, "\tcvt.s.w $f0, $f0\n");
        fprintf(out, "\ts.s $f0, 0($t2)");
    }
    if (t == DOUBLE) {
        fprintf(out, "\tmtc1 $t3, $f0\n");
        fprintf(out, "\tcvt.s.w $f0, $f0\n");
        fprintf(out, "\tcvt.d.s $f0, $f0\n");
        fprintf(out, "\ts.d $f0, 0($t2)\n");
    }
}


VariableMap map;
QuadTable text;
LabelTable lb;
int reg[32];
int i;

int main() {
    char code[100];
    char elements[10][100];
    memset(code, 0, sizeof(code));
    fp = fopen("/Users/petertam/Desktop/in.txt","r");
    out = fopen("/Users/petertam/Desktop/out.asm", "w");
    //定义变量 提取语句
    while (fgets(code, 100, fp) != NULL) {
        code[strlen(code)-1]='\0';
        int p = 0;
        int cnt = 0;
        memset(elements, 0, sizeof(elements));
        while (getElement(code, &p, elements[cnt]) > 0) cnt++;
        Quad quad;
        //如果三地址码定义了一个int
        if (strcmp(elements[0], "int")==0) {
            addVariable(&map, elements[1], INTEGER, elements[2]);
        }
        //如果三地址码定义了一个float
        if (strcmp(elements[0], "float")==0) {
            addVariable(&map, elements[1], FLOAT, elements[2]);
        }
        //如果三地址码定义了一个string
        if (strcmp(elements[0], "string")==0) {
            addVariable(&map, elements[1], STRING, elements[2]);
        }
        //如果三地址码定义了一个double
        if (strcmp(elements[0], "double")==0) {
            addVariable(&map, elements[1], DOUBLE, elements[2]);
        }
        //如果三地址码第一个元素是malloc
        if (strcmp(elements[0], "malloc")==0) {
            addArray(&map, elements[1], atoi(elements[2]));
        }
        if (strcmp(elements[0], "call")==0) {
            strcpy(quad.addr1.contents.name, elements[1]);
            quad.addr1.kind = String;
            quad.op = call;
            text.quad[text.num] = quad;
            text.num++;
        }
        //如果三地址码第一个元素是read
        if (strcmp(elements[0], "read")==0) {
            strcpy(quad.addr1.contents.name, elements[1]);
            quad.addr1.kind = getKind(elements[0]);
            quad.op = rd;
            text.quad[text.num] = quad;
            text.num++;
        }
        //如果三地址码第一个元素是print
        if (strcmp(elements[0], "print")==0) {
            //***有可能输出常量***记得补充
            strcpy(quad.addr1.contents.name, elements[1]);
            quad.addr1.kind = getKind(elements[1]);
            quad.op = pri;
            text.quad[text.num] = quad;
            text.num++;
        }
        //如果三地址码第一个元素是println
        if (strcmp(elements[0], "println")==0) {
            //***有可能输出常量***记得补充
            strcpy(quad.addr1.contents.name, elements[1]);
            quad.addr1.kind = getKind(elements[1]);
            quad.op = priln;
            text.quad[text.num] = quad;
            text.num++;
        }
        //判断语句
        if (strcmp(elements[0], "if_false")==0) {
            quad.addr1.kind = String;
            quad.addr2.kind = String;
            strcpy(quad.addr1.contents.name, elements[1]);
            strcpy(quad.addr2.contents.name, elements[3]);
            quad.op = if_f;
            text.quad[text.num] = quad;
            text.num++;
        }
        //添加一个标签
        if (strcmp(elements[0], "label")==0) {
            addLabel(elements[1], &lb);
            quad.op = lab;
            quad.addr1.kind = String;
            strcpy(quad.addr1.contents.name, elements[1]);
            text.quad[text.num] = quad;
            text.num++;
        }
        //改变指针类型
        if (strcmp(elements[0], "point")==0) {
            quad.op = dec;
            quad.addr1.kind = String;
            quad.addr2.kind = String;
            strcpy(quad.addr1.contents.name, elements[1]);
            strcpy(quad.addr2.contents.name, elements[2]);
            text.quad[text.num] = quad;
            text.num++;
        }
        //直接跳转语句
        if (strcmp(elements[0], "goto")==0) {
            quad.op = jump;
            strcpy(quad.addr1.contents.name, elements[1]);
            text.quad[text.num] = quad;
            text.num++;
        }
        //如果是函数名（那最好有个main)
        if (strcmp(elements[0], "entry")==0) {
            
        }
        //判断 + 跳转语句 if a == b then goto label 关键是拆成两句话
        if (strcmp(elements[0], "if")==0) {
            //先比较
            //eq赋值为相同
            if (strcmp(elements[2], "==")==0) quad.op = eq;
            //gt赋值为大于
            if (strcmp(elements[2], ">")==0) quad.op = gt;
            //gt赋值为小于
            if (strcmp(elements[2], "<")==0) quad.op = sm;
            quad.addr1.kind = getKind(elements[1]);
            quad.addr2.kind = getKind(elements[3]);
            quad.addr3.kind = String;
            if (quad.addr1.kind == IntConst) quad.addr1.contents.intVal = atoi(elements[1]);
            if (quad.addr1.kind == FloatConst) quad.addr1.contents.doubleVal = atof(elements[1]);
            if (quad.addr1.kind != IntConst && quad.addr1.kind != FloatConst) strcpy(quad.addr1.contents.name, elements[1]);

            if (quad.addr2.kind == IntConst) quad.addr2.contents.intVal = atoi(elements[3]);
            if (quad.addr2.kind == FloatConst) quad.addr2.contents.doubleVal = atof(elements[3]);
            if (quad.addr2.kind != IntConst && quad.addr2.kind != FloatConst) strcpy(quad.addr2.contents.name, elements[3]);
            strcpy(quad.addr3.contents.name, "$t7");
            text.quad[text.num] = quad;
            text.num++;
            //再跳转
            memset(&quad, 0, sizeof(Quad));
            quad.addr1.kind = String;
            quad.addr2.kind = String;
            strcpy(quad.addr1.contents.name, "$t7");
            strcpy(quad.addr2.contents.name, elements[6]);
            quad.op = if_f;
            text.quad[text.num] = quad;
            text.num++;
        }
        //赋值语句
        if (strcmp(elements[1], "=")==0) {
            quad.addr1.kind = getKind(elements[2]);
            quad.addr2.kind = getKind(elements[4]);
            if (quad.addr1.kind == IntConst) quad.addr1.contents.intVal = atoi(elements[2]);
            if (quad.addr1.kind == FloatConst) quad.addr1.contents.floatVal = atof(elements[2]);
            if (quad.addr1.kind != IntConst && quad.addr1.kind != FloatConst) strcpy(quad.addr1.contents.name, elements[2]);
            if (quad.addr2.kind == IntConst) quad.addr2.contents.intVal = atoi(elements[4]);
            if (quad.addr2.kind == FloatConst) quad.addr2.contents.floatVal = atof(elements[4]);
            if (quad.addr2.kind != IntConst && quad.addr2.kind != FloatConst) strcpy(quad.addr2.contents.name, elements[4]);
            //eq赋值为相同
            if (strcmp(elements[3], "==")==0) quad.op = eq;
            //gt赋值为大于
            if (strcmp(elements[3], ">")==0) quad.op = gt;
            //gt赋值为小于
            if (strcmp(elements[3], "<")==0) quad.op = sm;
            //mul乘法
            if (strcmp(elements[3], "*")==0) quad.op = mul;
            //add加法
            if (strcmp(elements[3], "+")==0) quad.op = add;
            //sub减法
            if (strcmp(elements[3], "-")==0) quad.op = sub;
            //dvd除法
            if (strcmp(elements[3], "/")==0) quad.op = dvd;
            //单纯赋值 根本没有计算
            if (strlen(elements[3]) == 0) {
                quad.op = asn;
                quad.addr2.kind = getKind(elements[0]);
                strcpy(quad.addr2.contents.name, elements[0]);
            }
            if (quad.op != asn) {
                quad.addr3.kind = getKind(elements[0]);
                strcpy(quad.addr3.contents.name, elements[0]);
            }
            text.quad[text.num] = quad;
            text.num++;
        }
    }
    //输出变量的MIPS
    declareVariable(&map);
    //处理开头
    fprintf(out, ".text\n\n");
    fprintf(out, "main:\n");
    //处理语句
    for (i=0; i<text.num; i++) {
        if (text.quad[i].op == asn) genASN(text.quad+i, &map);
        if (text.quad[i].op == pri || text.quad[i].op == priln) genPRINT(text.quad+i, &map);
        if (text.quad[i].op == rd) genREAD(text.quad+i, &map);
        if (text.quad[i].op == add || text.quad[i].op == mul || text.quad[i].op == sub || text.quad[i].op == dvd) genCAL(text.quad+i, &map);
        if (text.quad[i].op == gt || text.quad[i].op == sm || text.quad[i].op == eq) genCompare(text.quad+i, &map);
        if (text.quad[i].op == lab) genLABEL(text.quad+i);
        if (text.quad[i].op == if_f) genFJUMP(text.quad+i);
        if (text.quad[i].op == dec) changeType(text.quad+i, &map);
        if (text.quad[i].op == jump) genJUMP(text.quad+i);
    }
    //退出主程序
    fprintf(out, "\tjr $ra\n");
    fclose(fp);
    fclose(out);
    return 0;
}