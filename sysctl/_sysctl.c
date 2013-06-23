#include <sys/types.h>
#include <sys/sysctl.h>
#include <errno.h>

#include <Python.h>
#include <structmember.h>

typedef struct {
	PyObject_HEAD;
	PyObject *name;
	PyObject *value;
	/* Type-specific fields go here. */
} Sysctl;


static int Sysctl_init(Sysctl *self, PyObject *args, PyObject *kwds) {

	PyObject *name=NULL, *value=NULL, *tmp;
	static char *kwlist[] = {"name", "value", NULL};
	if (! PyArg_ParseTupleAndKeywords(args, kwds, "OO", kwlist, &name, &value))
		return -1;

	if(name) {
		tmp = self->name;
		Py_INCREF(name);
		self->name = name;
		Py_XDECREF(tmp);
	}
	if(value) {
		tmp = self->value;
		Py_INCREF(value);
		self->value = value;
		Py_XDECREF(tmp);
	}

	return 0;

}

static PyMemberDef Sysctl_members[] = {
	{"name", T_OBJECT_EX, offsetof(Sysctl, name), READONLY, "name"},
	{"value", T_OBJECT_EX, offsetof(Sysctl, value), 0, "value"},
	{NULL}  /* Sentinel */
};

static PyTypeObject SysctlType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /*ob_size*/
	"Sysctl",                  /*tp_name*/
	sizeof(Sysctl),            /*tp_basicsize*/
	0,                         /*tp_itemsize*/
	0,                         /*tp_dealloc*/
	0,                         /*tp_print*/
	0,                         /*tp_getattr*/
	0,                         /*tp_setattr*/
	0,                         /*tp_compare*/
	0,                         /*tp_repr*/
	0,                         /*tp_as_number*/
	0,                         /*tp_as_sequence*/
	0,                         /*tp_as_mapping*/
	0,                         /*tp_hash */
	0,                         /*tp_call*/
	0,                         /*tp_str*/
	0,                         /*tp_getattro*/
	0,                         /*tp_setattro*/
	0,                         /*tp_as_buffer*/
	Py_TPFLAGS_DEFAULT,        /*tp_flags*/
	"Sysctl objects",           /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	0, //Noddy_methods,             /* tp_methods */
	Sysctl_members,             /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)Sysctl_init,      /* tp_init */
	0,                         /* tp_alloc */
	//Sysctl_new,                 /* tp_new */
};


static u_int sysctl_type(int *oid, int len) {

	int qoid[CTL_MAXNAME+2], i;
	u_char buf[BUFSIZ];
	size_t j;

	qoid[0] = 0;
	qoid[1] = 4;
	memcpy(qoid + 2, oid, len * sizeof(int));

	j = sizeof(buf);
	i = sysctl(qoid, len + 2, buf, &j, 0, 0);
	if(i < 0) exit(-1);

	return *(u_int *) buf;
}


static PyObject *new_sysctlobj(int *oid, int nlen) {

	char name[BUFSIZ];
	int qoid[CTL_MAXNAME+2], ctltype;
	u_char *val;
	size_t j, len;
	u_int kind;
	PyObject *sysctlObj, *args, *kwargs, *value;

	bzero(name, BUFSIZ);
	qoid[0] = 0;
	qoid[1] = 1;
	memcpy(qoid + 2, oid, nlen * sizeof(int));

	j = sizeof(name);
	sysctl(qoid, nlen + 2, name, &j, 0, 0);
	kind = sysctl_type(oid, nlen);
	ctltype = kind & CTLTYPE;

	j = 0;
	sysctl(oid, nlen, 0, &j, 0, 0);
	j += j; /* double size just to be sure */

	val = malloc(j + 1);
	len = j;

	sysctl(oid, nlen, val, &len, 0, 0);

	switch(ctltype) {
		case CTLTYPE_STRING:
			val[len] = '\0';
			value = PyString_FromString((char *)val);
			break;
		case CTLTYPE_INT:
			value = PyInt_FromLong( *(int *) val);
			break;
		case CTLTYPE_UINT:
			value = PyInt_FromLong( *(u_int *) val);
			break;
		case CTLTYPE_LONG:
			value = PyLong_FromLong( *(long *) val);
			break;
		case CTLTYPE_ULONG:
			value = PyLong_FromUnsignedLong( *(u_long *) val);
			break;
		case CTLTYPE_S64:
			value = PyLong_FromLongLong( *(long long *) val);
			break;
		case CTLTYPE_U64:
			value = PyLong_FromUnsignedLongLong( *(unsigned long long *) val);
			break;
		default:
			value = PyString_FromString("NOT IMPLEMENTED");
			break;
	}

	args = Py_BuildValue("()");
	kwargs = Py_BuildValue("{s:s,s:O}", "name", name, "value", value);
	sysctlObj = PyObject_Call((PyObject *)&SysctlType, args, kwargs);

	return sysctlObj;

}

static PyObject* sysctl_all(PyObject* self, PyObject* args) {

	int name1[22], name2[22], i, j, *oid = NULL, len = 0;
	size_t l1, l2;
	PyObject *list;

	if(!PyArg_ParseTuple(args, "I", &len))
		return NULL;

	list = PyList_New(0);

	name1[0] = 0;
	name1[1] = 2;
	l1 = 2;
	if (len) {
		memcpy(name1+2, oid, len * sizeof(int));
		l1 += len;
	} else {
		name1[2] = 1;
		l1++;
	}
	for (;;) {
		l2 = sizeof(name2);
		j = sysctl(name1, l1, name2, &l2, 0, 0);
		if (j < 0) {
			if (errno == ENOENT)
				return list;
			//else
			//	err(1, "sysctl(getnext) %d %zu", j, l2);
		}

		l2 /= sizeof(int);

		if (len < 0 || l2 < (unsigned int)len)
			return list;

		for (i = 0; i < len; i++)
			if (name2[i] != oid[i])
				return list;

		//i = show_var(name2, l2);
		PyList_Append(list, new_sysctlobj(name2, l2));

		memcpy(name1+2, name2, l2 * sizeof(int));
		l1 = 2 + l2;
	}

	return list;

}

static PyMethodDef SysctlMethods[] = {
	{"sysctl_all", (PyCFunction) sysctl_all, METH_VARARGS, "Sysctl all"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
init_sysctl(void) {
	PyObject *m;


	SysctlType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&SysctlType) < 0)
		return;

	m = Py_InitModule("_sysctl", SysctlMethods);
	if (m == NULL)
		return;

	Py_INCREF(&SysctlType);
	PyModule_AddObject(m, "Sysctl", (PyObject *)&SysctlType);

}
