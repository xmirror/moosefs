PORTVERSION=	%%VERSION%%
PORTREVISION=	1

MASTER_SITES=	http://www.moosefs.org/tl_files/mfscode/

LICENSE=	PROPRIETARY
LICENSE_NAME=	Proprietary Open Source License
LICENSE_PERMS=	dist-mirror dist-sell pkg-mirror pkg-sell auto-accept
LICENSE_FILE=	${WRKSRC}/COPYING

GNU_CONFIGURE=	yes

MFS_USER=	mfs
MFS_GROUP=	mfs
MFS_WORKDIR=	/var
MFS_CGIDIR=	${PREFIX}/share/mfscgi
MFS_CGISERVDIR=	${PREFIX}/sbin

.if ${PORTNAME}==moosefs
DISTNAME=	${PORTNAME}-ce-${PORTVERSION}-${PORTREVISION}
WRKSRC=		${WRKDIR}/${PORTNAME}-ce-${PORTVERSION}
.else
DISTNAME=	${PORTNAME}-${PORTVERSION}-${PORTREVISION}
WRKSRC=		${WRKDIR}/${PORTNAME}-${PORTVERSION}
.endif

MAINTAINER=	acid@moosefs.com

USERS=		${MFS_USER}
GROUPS=		${MFS_GROUP}

CONFIGURE_ARGS+=	--localstatedir=${MFS_WORKDIR} \
			--with-default-user=${MFS_USER} \
			--with-default-group=${MFS_GROUP} \
			--with-mfscgiserv-dir=${MFS_CGISERVDIR} \
			--with-mfscgi-dir=${MFS_CGIDIR}
