import nextcord
from nextcord.ext import commands
from nextcord.ui import Button, View

class PageView(View):
    def __init__(self, data: list):
        super().__init__
        self.data = data
        self.current_page = 0
        self.update_buttons()

    def update_buttons(self):
        # Updates the button states
        self.clear_items()

        if self.current_page > 0:
            self.add_item(Button(style=nextcord.ButtonStyle.primary, label="<<", callback=self.previous_page))
        if self.current_page < len(self.data) - 1:
            self.add_item(Button(style=nextcord.ButtonStyle.primary, label=">>", callback=self.next_page))
        
    async def send_initial_message(self, ctx: commands.Context):
        # Sends the initial message
        return await ctx.send(self.data[self.current_page], view=self)
    
    async def previous_page(self, interaction: nextcord.Interaction):
        # Handles the << button click
        if self.current_page > 0:
            self.current_page -= 1
            await interaction.response.edit_message(content=self.data[self.current_page], view=self)
            self.update_buttons()
    
    async def next_page(self, interaction: nextcord.Interaction):
        # Handles the >> button click
        if self.current_page < len(self.data) - 1:
            self.current_page += 1
            await interaction.response.edit_message(content=self.data[self.current_page], view=self)
            self.update_buttons()
    
