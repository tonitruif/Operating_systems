/* Написать программу, которая создавала бы процесс-демон с помощью функций Demonize, Already running, Main  */
#include <syslog.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h> //umask
#include <unistd.h> //setsid
#include <stdio.h> //perror
#include <signal.h> //sidaction
#include <string.h>
#include <errno.h>
#include <sys/file.h>
#include <time.h>

#define LOCKFILE "/var/run/daemon.pid"  // директория с правами суперюзера
#define LOCKMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

//S_IRUSR - user has read permission
//S_IWUSR  00200 user has write permission
//S_IRGRP  00040 group has read permission
//S_IROTH  00004 others have read permission

extern int lockfile(int);

int already_running(void)
{
    int fd;
    char    buf[16];
    fd = open(LOCKFILE, O_RDWR|O_CREAT, LOCKMODE); if (fd < 0) {
        syslog(LOG_ERR, "невозможно открыть %s: %s", LOCKFILE, strerror(errno));
        exit(1); }
    if (lockfile(fd) < 0) {
        if (errno == EACCES || errno == EAGAIN) {
            close(fd);
            return(1); } //демон уже запущен
        syslog(LOG_ERR, "невозможно установить блокировку на %s: %s", LOCKFILE, strerror(errno));
        exit(1); }
    ftruncate(fd, 0);
    sprintf(buf, "%ld", (long)getpid());
    write(fd, buf, strlen(buf)+1);
    return(0);
}

void daemonize(const char *cmd)
{
    int fd0, fd1, fd2;
    pid_t pid;
    struct rlimit rl;
    struct sigaction sa;

    // 1. Сбрасывание маски режима создания файла
    umask(0);
    // Потомок наследует сброшенную маску
    // Маска 0 - все биты прав доступа, чтобы де
    // 2. Получение максимального возможного номера дискриптора
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        perror("Невозможно получить максимальный номер дискриптора!\n");

    // 3. Стать лидером новой сессии, чтобы утратить управляющий терминал
    if ((pid = fork()) < 0)
        perror("Ошибка функции fork!\n");
    else if (pid != 0) //родительский процесс
        exit(0);

    setsid(); // Системный вызов, когда демон теряет терминал

    // 4. Обеспечение невозможности обретения терминала в будущем
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        perror("Невозможно игнорировать сигнал SIGHUP!\n");

     if ((pid = fork()) < 0)
         perror("Ошибка функции fork!\n");
     else if (pid != 0) //родительский процесс
         exit(0);

    // 5. Назначить корневой каталог текущим рабочим каталогом,
    // чтобы впоследствии можно было отмонтировать файловую систему
    if (chdir("/") < 0)
        perror("Невозможно назначить корневой каталог текущим рабочим каталогом!\n");

    // 6. Зактрыть все файловые дескрипторы
    if (rl.rlim_max == RLIM_INFINITY)
        rl.rlim_max = 1024;
    for (int i = 0; i < rl.rlim_max; i++)
        close(i);

    // 7. Присоеденить файловые дескрипторы 0, 1, 2 к /dev/null
    fd0 = open("/dev/null", O_RDWR);
    fd1 = dup(0); //копируем файловый дискриптор
    fd2 = dup(0);

    // 8. Инициализировать файл журнала
    openlog(cmd, LOG_CONS, LOG_DAEMON);
    if (fd0 != 0 || fd1 != 1 || fd2 != 2)
    {
        syslog(LOG_ERR, "ошибочные файловые дескрипторы %d %d %d\n", fd0, fd1, fd2);
        exit(1);
    }

    syslog(LOG_WARNING, "Демон запущен!");

}

int main()
{
    daemonize("my_daemon");
    time_t timer;
    // Блокировка файла для одной существующей копии демона
    if (already_running() != 0)
    {
        syslog(LOG_ERR, "Демон уже запущен\n");
        exit(1);
    }

    syslog(LOG_WARNING, "Проверка пройдена");
    while(1)
    {
        timer = time(NULL);
        syslog(LOG_ERR, "time: %s\n", ctime(&timer));

        sleep(2);
    }


}
