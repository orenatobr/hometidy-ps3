#include "texts.h"

namespace strings {
namespace {

Lang g_lang = Lang::En;

struct Row {
    const char* en;
    const char* pt;
};

// Same order as enum Id.
const Row kRows[] = {
    {"Tidy up your PS3 game library", "Organize a biblioteca de jogos do seu PS3"},
    {"Reading your library...", "Lendo sua biblioteca..."},
    {"Working...", "Trabalhando..."},
    {"Sort game list", "Ordenar a lista de jogos"},
    {"Puts the webMAN game list in order: A to Z, and the games of one series by release date.",
     "Coloca a lista de jogos do webMAN em ordem: de A a Z, e os jogos de uma série por data de lançamento."},
    {"Mark as completed", "Marcar como zerado"},
    {"Add a green check to the cover of a game you finished. You can remove it later.",
     "Põe um check verde na capa de um jogo que você zerou. Dá para remover depois."},
    {"%d games in the list", "%d jogos na lista"},
    {"%d games found", "%d jogos encontrados"},
    {"Select", "Escolher"},
    {"Back", "Voltar"},
    {"Exit", "Sair"},
    {"Language", "Idioma"},
    {"Apply", "Aplicar"},
    {"Platform", "Plataforma"},
    {"Scroll", "Rolar"},
    {"Page", "Página"},
    {"Mark", "Marcar"},
    {"Unmark", "Desmarcar"},
    {"Cancel", "Cancelar"},
    {"Close app", "Fechar o app"},
    {"%d games", "%d jogos"},
    {"%d to move", "%d a mover"},
    {"Already sorted", "Já está em ordem"},
    {"was %d", "era %d"},
    {"Everything is already in order. Nothing to write.", "Tudo já está em ordem. Nada a gravar."},
    {"The webMAN game list (mygames.xml) was not found, or it is not in a format this app knows.",
     "A lista de jogos do webMAN (mygames.xml) não foi encontrada, ou não está em um formato que este app conhece."},
    {"Game list file", "Arquivo da lista de jogos"},
    {"Apply the new order?", "Aplicar a nova ordem?"},
    {"A backup of the current list is saved first. Only the order changes: titles, covers and paths stay as they are.",
     "Antes disso, um backup da lista atual é salvo. Só a ordem muda: títulos, capas e caminhos continuam iguais."},
    {"New order saved", "Nova ordem salva"},
    {"Close this app, then open the game column again. If the old order is still there, refresh the list in webMAN.",
     "Feche este app e abra a coluna de jogos de novo. Se a ordem antiga continuar, atualize a lista no webMAN."},
    {"Backup", "Backup"},
    {"Checked again after 2 seconds: still there.", "Conferido de novo após 2 segundos: continua lá."},
    {"Checked again after 2 seconds: webMAN changed the file back! Sort again after webMAN finishes.",
     "Conferido de novo após 2 segundos: o webMAN desfez a mudança! Ordene de novo quando ele terminar."},
    {"Could not finish", "Não foi possível concluir"},
    {"The file could not be read.", "Não foi possível ler o arquivo."},
    {"The file changed while this screen was open. Go back and try again.",
     "O arquivo mudou com esta tela aberta. Volte e tente de novo."},
    {"Could not save the backup. Nothing was changed.", "Não foi possível salvar o backup. Nada foi alterado."},
    {"Could not write the file. The backup was saved.", "Não foi possível gravar o arquivo. O backup foi salvo."},
    {"The file read back different from what was written.", "O arquivo lido de volta difere do que foi gravado."},
    {"Nothing to do.", "Nada a fazer."},
    {"Nothing says this cover has a mark to remove.", "Nada indica que esta capa tenha uma marca para remover."},
    {"This cover is not a PNG or JPEG of a usable size.", "Esta capa não é um PNG ou JPEG de tamanho utilizável."},
    {"Completed", "Zerado"},
    {"Not marked", "Sem marca"},
    {"No cover image", "Sem imagem de capa"},
    {"Marked as completed", "Marcado como zerado"},
    {"Mark removed", "Marca removida"},
    {"Mark removed (approximate: no original was saved)", "Marca removida (aproximada: não havia original salvo)"},
    {"Already marked", "Já está marcado"},
    {"This game has no cover image to mark.", "Este jogo não tem imagem de capa para marcar."},
    {"No games with a PS3ISO, PS2ISO or PSXISO image were found.",
     "Nenhum jogo com imagem em PS3ISO, PS2ISO ou PSXISO foi encontrado."},
    {"Could not open the console storage (/dev_hdd0).", "Não foi possível abrir o armazenamento do console (/dev_hdd0)."},
};

static_assert(sizeof(kRows) / sizeof(kRows[0]) == static_cast<int>(Id::Count_), "string table out of sync");

}  // namespace

Lang current() { return g_lang; }
void set(Lang lang) { g_lang = lang; }

const char* get(Id id) {
    const Row& r = kRows[static_cast<int>(id)];
    return g_lang == Lang::Pt ? r.pt : r.en;
}

}  // namespace strings
